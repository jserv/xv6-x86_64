#include "types.h"
#include "defs.h"
#include "fs.h"
#include "file.h"
#include "cpuid.h"

uint maxleaf;
uint vendor[3];
// leaf = 1
uint version, processor, featuresExt, features;
// leaf = 7
uint sef_flags;

static void
cpu_printfeatures(void)
{
  uchar vendorStr[13];
  *(uint*)(&vendorStr[0]) = vendor[0];
  *(uint*)(&vendorStr[4]) = vendor[1];
  *(uint*)(&vendorStr[8]) = vendor[2];
  vendorStr[12] = 0;

  cprintf("CPU vendor: %s\n", vendorStr);
  cprintf("Max leaf: 0x%x\n", maxleaf);
  if (maxleaf >= 1) {
    cprintf("Features: ");
    PRINT_FEATURE(features, FPU);
    PRINT_FEATURE(features, VME);
    PRINT_FEATURE(features, DE);
    PRINT_FEATURE(features, PSE);
    PRINT_FEATURE(features, TSC);
    PRINT_FEATURE(features, MSR);
    PRINT_FEATURE(features, PAE);
    PRINT_FEATURE(features, MCE);
    PRINT_FEATURE(features, CX8);
    PRINT_FEATURE(features, APIC);
    PRINT_FEATURE(features, SEP);
    PRINT_FEATURE(features, MTRR);
    PRINT_FEATURE(features, PGE);
    PRINT_FEATURE(features, MCA);
    PRINT_FEATURE(features, CMOV);
    PRINT_FEATURE(features, PAT);
    PRINT_FEATURE(features, PSE36);
    PRINT_FEATURE(features, PSN);
    PRINT_FEATURE(features, CLFSH);
    PRINT_FEATURE(features, DS);
    PRINT_FEATURE(features, ACPI);
    PRINT_FEATURE(features, MMX);
    PRINT_FEATURE(features, FXSR);
    PRINT_FEATURE(features, SSE);
    PRINT_FEATURE(features, SSE2);
    PRINT_FEATURE(features, SS);
    PRINT_FEATURE(features, HTT);
    PRINT_FEATURE(features, TM);
    PRINT_FEATURE(features, PBE);

    cprintf("\nExt Features: ");
    PRINT_FEATURE(featuresExt, SSE3);
    PRINT_FEATURE(featuresExt, PCLMULQDQ);
    PRINT_FEATURE(featuresExt, DTES64);
    PRINT_FEATURE(featuresExt, MONITOR);
    PRINT_FEATURE(featuresExt, DS_CPL);
    PRINT_FEATURE(featuresExt, VMX);
    PRINT_FEATURE(featuresExt, SMX);
    PRINT_FEATURE(featuresExt, EIST);
    PRINT_FEATURE(featuresExt, TM2);
    PRINT_FEATURE(featuresExt, SSSE3);
    PRINT_FEATURE(featuresExt, CNXT_ID);
    PRINT_FEATURE(featuresExt, FMA);
    PRINT_FEATURE(featuresExt, CMPXCHG16B);
    PRINT_FEATURE(featuresExt, xTPR);
    PRINT_FEATURE(featuresExt, PDCM);
    PRINT_FEATURE(featuresExt, PCID);
    PRINT_FEATURE(featuresExt, DCA);
    PRINT_FEATURE(featuresExt, SSE4_1);
    PRINT_FEATURE(featuresExt, SSE4_2);
    PRINT_FEATURE(featuresExt, x2APIC);
    PRINT_FEATURE(featuresExt, MOVBE);
    PRINT_FEATURE(featuresExt, POPCNT);
    PRINT_FEATURE(featuresExt, TSCD);
    PRINT_FEATURE(featuresExt, AESNI);
    PRINT_FEATURE(featuresExt, XSAVE);
    PRINT_FEATURE(featuresExt, OSXSAVE);
    PRINT_FEATURE(featuresExt, AVX);
    PRINT_FEATURE(featuresExt, F16C);
    PRINT_FEATURE(featuresExt, RDRAND);
    cprintf("\n");
  }

  if (maxleaf >= 7) {
    cprintf("Structured Extended Features: ");
    PRINT_SEFEATURE(sef_flags, FSGSBASE);
    PRINT_SEFEATURE(sef_flags, TAM);
    PRINT_SEFEATURE(sef_flags, SMEP);
    PRINT_SEFEATURE(sef_flags, EREP);
    PRINT_SEFEATURE(sef_flags, INVPCID);
    PRINT_SEFEATURE(sef_flags, QM);
    PRINT_SEFEATURE(sef_flags, FPUCS);
    cprintf("\n");
  }
}

static void
cpuinfo(void)
{
  // check for CPUID support by setting and clearing ID (bit 21) in EFLAGS

  // When EAX=0, the processor returns the highest value (maxleaf) recognized for processor information
  asm("cpuid" : "=a"(maxleaf), "=b"(vendor[0]), "=c"(vendor[2]), "=d"(vendor[1]) : "a" (0) :);


  if (maxleaf >= 1) {
    // get model, family, stepping info
    asm("cpuid" : "=a"(version), "=b"(processor), "=c"(featuresExt), "=d"(features) : "a" (1) :);
  }

  if (maxleaf >= 2) {
    // cache and TLB info
  }

  if (maxleaf >= 3) {
    // processor serial number
  }

  if (maxleaf >= 4) {
    // deterministic cache parameters
  }

  if (maxleaf >= 5) {
    // MONITOR and MWAIT instructions
  }

  if (maxleaf >= 6) {
    // thermal and power management
  }

  if (maxleaf >= 7) {
    // structured extended feature flags (ECX=0)
    uint maxsubleaf;
    asm("cpuid" : "=a"(maxsubleaf), "=b"(sef_flags) : "a" (7), "c" (0) :);
  }

  /* ... and many more ... */
}

static int cpuid_read(struct inode* i, char* buf, int count)
{
   cpu_printfeatures();

   return 0;
}

static int cpuid_write(struct inode* i, char* buf, int count)
{
   cprintf("cpuid_write\n");
   return 0;
}

void
cpuidinit(void)
{
  devsw[CPUID].write = cpuid_write;
  devsw[CPUID].read = cpuid_read;

  cpuinfo();
}
