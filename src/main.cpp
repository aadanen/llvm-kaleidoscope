#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/StandardInstrumentations.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include <KaleidoscopeJIT.h>
#include <algorithm>
#include <ast.h>
#include <cassert>
#include <cctype>
#include <codegen.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <lexer.h>
#include <map>
#include <memory>
#include <parser.h>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::sys;
using namespace llvm::orc;

std::unique_ptr<KaleidoscopeJIT> TheJIT;
bool GENERATE_OBJECT_FILE = false;
DataLayout layout;

static ExitOnError ExitOnErr;
//===----------------------------------------------------------------------===//
// Top-Level parsing and JIT Driver
//===----------------------------------------------------------------------===//

void InitializeModuleAndManagers() {
  // Open a new context and module.
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("Kalos", *TheContext);
  TheModule->setDataLayout(layout);

  // Create a new builder for the module.
  Builder = std::make_unique<IRBuilder<>>(*TheContext);

  // Create new pass and analysis managers.
  TheFPM = std::make_unique<FunctionPassManager>();
  TheLAM = std::make_unique<LoopAnalysisManager>();
  TheFAM = std::make_unique<FunctionAnalysisManager>();
  TheCGAM = std::make_unique<CGSCCAnalysisManager>();
  TheMAM = std::make_unique<ModuleAnalysisManager>();
  ThePIC = std::make_unique<PassInstrumentationCallbacks>();
  TheSI = std::make_unique<StandardInstrumentations>(*TheContext,
                                                     /*DebugLogging*/ true);
  TheSI->registerCallbacks(*ThePIC, TheMAM.get());

  // Add transform passes.
  // Promote allocas to registers.
  TheFPM->addPass(PromotePass());
  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->addPass(InstCombinePass());
  // Reassociate expressions.
  TheFPM->addPass(ReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->addPass(GVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->addPass(SimplifyCFGPass());

  // Register analysis passes used in these transform passes.
  PassBuilder PB;
  PB.registerModuleAnalyses(*TheMAM);
  PB.registerFunctionAnalyses(*TheFAM);
  PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      if (!GENERATE_OBJECT_FILE) {
        fprintf(stderr, "Read function definition:");
        FnIR->print(errs());
        fprintf(stderr, "\n");
        ExitOnErr(TheJIT->addModule(
            ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
        InitializeModuleAndManagers();
      }
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      if (!GENERATE_OBJECT_FILE) {
        fprintf(stderr, "Read extern: ");
        FnIR->print(errs());
        fprintf(stderr, "\n");
        FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
      }
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      if (!GENERATE_OBJECT_FILE) {
        // Create a ResourceTracker to track JIT'd memory
        // allocated to our anonymous expression -- that way we
        // can free it after executing.
        auto RT = TheJIT->getMainJITDylib().createResourceTracker();

        auto TSM =
            ThreadSafeModule(std::move(TheModule), std::move(TheContext));
        ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
        InitializeModuleAndManagers();

        // Search the JIT for the __anon_expr symbol.
        auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

        // Get the symbol's address and cast it to the right
        // type (takes no arguments, returns a double) so we can
        // call it as a native function.
        double (*FP)() = ExprSymbol.toPtr<double (*)()>();
        fprintf(stderr, "Evaluated to %f\n", FP());

        // Delete the anonymous expression module from the JIT.
        ExitOnErr(RT->remove());
      }
    }
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
void MainLoop() {
  while (true) {
    if (USE_REPL) {
      fprintf(stderr, "ready> ");
    }
    switch (CurTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    case tok_extern:
      HandleExtern();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

void printVersion(raw_ostream &out) { out << "Kalos Version 1.0\n"; }
//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main(int argc, char **argv) {
  cl::SetVersionPrinter(printVersion);
  cl::OptionCategory KalosCategory(
      "Kalos Options", "Options for controlling the compilation process");
  cl::opt<std::string> OutputFilename("o", cl::desc("specify output filename"),
                                      cl::value_desc("filename"), cl::Optional,
                                      cl::cat(KalosCategory), cl::init(""));
  cl::list<std::string> InputFilenames(cl::Positional,
                                       cl::desc("[input files]"),
                                       cl::ZeroOrMore, cl::cat(KalosCategory));
  cl::HideUnrelatedOptions(KalosCategory);
  cl::ParseCommandLineOptions(argc, argv, "Kalos\n");

  if (InputFilenames.end() > InputFilenames.begin()) {
    USE_REPL = false;
    // for now only compile the first file
    std::string fname = *(InputFilenames.begin());
    std::ifstream f(fname);
    auto sz = std::filesystem::file_size(fname);
    source.resize(sz);

    f.read(&source[0], sz);
  } else {
    USE_REPL = true;
  }

  GENERATE_OBJECT_FILE = OutputFilename.size() > 0;

  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  if (!GENERATE_OBJECT_FILE) {
    if (USE_REPL)
      fprintf(stderr, "ready> ");

    getNextToken();
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    InitializeNativeTargetAsmParser();

    TheJIT = ExitOnErr(KaleidoscopeJIT::Create());
    layout = TheJIT->getDataLayout();

    InitializeModuleAndManagers();

    // Run the main "interpreter loop" now.
    MainLoop();
    return 0;
  } else {
    getNextToken();
    auto TargetTriple = sys::getDefaultTargetTriple();

    InitializeAllTargetInfos();
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmParsers();
    InitializeAllAsmPrinters();

    std::string Error;
    auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
    if (!Target) {
      errs() << Error;
      return 1;
    }

    auto CPU = "generic";
    auto Features = "";
    TargetOptions opt;
    auto TargetMachine = Target->createTargetMachine(
        Triple(TargetTriple), CPU, Features, opt, Reloc::PIC_);
    layout = TargetMachine->createDataLayout();
    InitializeModuleAndManagers();

    TheModule->setDataLayout(layout);
    TheModule->setTargetTriple(Triple(TargetTriple));

    std::error_code EC;
    raw_fd_ostream dest(OutputFilename, EC, sys::fs::OF_None);

    if (EC) {
      errs() << "Could not open file: " << EC.message();
      return 1;
    }

    legacy::PassManager pass;
    auto FileType = CodeGenFileType::ObjectFile;
    if (TargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
      errs() << "TargetMachine can't emit a file of this type";
      return 1;
    }

    MainLoop();

    pass.run(*TheModule);
    dest.flush();
    return 0;
  }

  return 0;
}
