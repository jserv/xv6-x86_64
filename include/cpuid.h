#ifndef _CPUID__H
#define _CPUID__H

#define PRINT_FEATURE(_flags, _feature) \
	if (_flags & CPUID_LEAF_1_##_feature) cprintf(#_feature " ");
#define PRINT_SEFEATURE(_flags, _feature) \
	if (_flags & CPUID_LEAF_7_##_feature) cprintf(#_feature " ");

/* Vol 1 chapter 14.1: Processor Identification and Feature Determination */
/* Vol 2A chapter 3: */
#define INTEL_PROC_FAMILY_SHIFT 8
#define INTEL_PROC_FAMILY_MASK (0xF<<INTEL_PROC_FAMILY_SHIFT)
#define INTEL_PROC_FAMILY_PENTIUM_4 0xF
#define CPUID_LEAF_1_FPU (1<<0) // floating point unit
#define CPUID_LEAF_1_VME (1<<1) // Virtual 8086 Mode Enhancements.
#define CPUID_LEAF_1_DE (1<<2) // Debugging Extensions.
#define CPUID_LEAF_1_PSE (1<<3) // Page Size Extension.
#define CPUID_LEAF_1_TSC (1<<4) // Time Stamp Counter.
#define CPUID_LEAF_1_MSR (1<<5) // Model Specific Registers RDMSR and WRMSR Instructions.
#define CPUID_LEAF_1_PAE (1<<6) // Physical Address Extension.
#define CPUID_LEAF_1_MCE (1<<7) // Machine Check Exception.
#define CPUID_LEAF_1_CX8 (1<<8) // CMPXCHG8B Instruction
#define CPUID_LEAF_1_APIC (1<<9) // APIC On-Chip
#define CPUID_LEAF_1_SEP (1<<11) // SYSENTER and SYSEXIT Instructions
#define CPUID_LEAF_1_MTRR (1<<12) // Memory Type Range Registers
#define CPUID_LEAF_1_PGE (1<<13) // Page Global Bit
#define CPUID_LEAF_1_MCA (1<<14) // Machine Check Architecture
#define CPUID_LEAF_1_CMOV (1<<15) // Conditional Move Instructions
#define CPUID_LEAF_1_PAT (1<<16) // Page Attribute Table
#define CPUID_LEAF_1_PSE36 (1<<17) // 36-Bit Page Size Extension
#define CPUID_LEAF_1_PSN (1<<18) // Processor Serial Number
#define CPUID_LEAF_1_CLFSH (1<<19) // CLFLUSH Instruction
#define CPUID_LEAF_1_DS (1<<21) // Debug Store
#define CPUID_LEAF_1_ACPI (1<<22) // Thermal Monitor and Software Controlled Clock Facilities
#define CPUID_LEAF_1_MMX (1<<23) // Intel MMX Technology
#define CPUID_LEAF_1_FXSR (1<<24) // FXSAVE and FXRSTOR Instructions
#define CPUID_LEAF_1_SSE (1<<25) // SSE
#define CPUID_LEAF_1_SSE2 (1<<26) // SSE2
#define CPUID_LEAF_1_SS (1<<27) // Self Snoop
#define CPUID_LEAF_1_HTT (1<<28) // Max APIC IDs reserved field is Valid
#define CPUID_LEAF_1_TM (1<<29) // Thermal Monitor
#define CPUID_LEAF_1_PBE (1<<31) // Pending Break Enable

#define CPUID_LEAF_1_SSE3 (1<<0) // Streaming SIMD Extensions 3 (SSE3)
#define CPUID_LEAF_1_PCLMULQDQ (1<<1) // PCLMULQDQ
#define CPUID_LEAF_1_DTES64 (1<<2) // 64-bit DS Area
#define CPUID_LEAF_1_MONITOR (1<<3) // MONITOR/MWAIT
#define CPUID_LEAF_1_DS_CPL (1<<4) // CPL Qualified Debug Store
#define CPUID_LEAF_1_VMX (1<<5) // Virtual Machine Extensions
#define CPUID_LEAF_1_SMX (1<<6) // Safer Mode Extensions
#define CPUID_LEAF_1_EIST (1<<7) // Enhanced Intel SpeedStep® technology
#define CPUID_LEAF_1_TM2 (1<<8) // Thermal Monitor 2
#define CPUID_LEAF_1_SSSE3 (1<<9) // Supplemental Streaming SIMD Extensions 3 (SSSE3)
#define CPUID_LEAF_1_CNXT_ID (1<<10) // L1 Context ID
#define CPUID_LEAF_1_FMA (1<<12) // supports FMA extensions using YMM state
#define CPUID_LEAF_1_CMPXCHG16B (1<<13) // CMPXCHG16B Available
#define CPUID_LEAF_1_xTPR (1<<14) // xTPR Update Control
#define CPUID_LEAF_1_PDCM (1<<15) // Perfmon and Debug Capability
#define CPUID_LEAF_1_PCID (1<<17) // Process-context identifiers
#define CPUID_LEAF_1_DCA (1<<18) // prefetch data from a memory mapped device
#define CPUID_LEAF_1_SSE4_1 (1<<19) //
#define CPUID_LEAF_1_SSE4_2 (1<<20)
#define CPUID_LEAF_1_x2APIC (1<<21)
#define CPUID_LEAF_1_MOVBE (1<<22)
#define CPUID_LEAF_1_POPCNT (1<<23)
#define CPUID_LEAF_1_TSCD (1<<24) // local APIC timer supports one-shot operation using a TSC deadline value
#define CPUID_LEAF_1_AESNI (1<<25)
#define CPUID_LEAF_1_XSAVE (1<<26) // supports the XSAVE/XRSTOR processor extended states
#define CPUID_LEAF_1_OSXSAVE (1<<27) // the OS has enabled XSETBV/XGETBV instructions
#define CPUID_LEAF_1_AVX (1<<28) //
#define CPUID_LEAF_1_F16C (1<<29) // 16-bit floating-point conversion instructions
#define CPUID_LEAF_1_RDRAND (1<<30)
#define GET_STEPPING(_a) (_a & 0xF)
#define GET_MODEL(_a) ((_a >> 4) & 0xF)
#define GET_FAMILY(_a) ((_a >> 8) & 0xF)

#define CPUID_LEAF_7_FSGSBASE (1<<0) // Supports RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE if 1.
#define CPUID_LEAF_7_TAM (1<<1) // IA32_TSC_ADJUST MSR is supported if 1.
#define CPUID_LEAF_7_SMEP (1<<7) // Supports Supervisor Mode Execution Protection if 1.
#define CPUID_LEAF_7_EREP (1<<9) // Supports Enhanced REP MOVSB/STOSB if 1.
#define CPUID_LEAF_7_INVPCID (1<<10) // If 1, supports INVPCID instruction for system software that manages process-context identifiers.
#define CPUID_LEAF_7_QM (1<12) // Supports Quality of Service Monitoring (QM) capability if 1.
#define CPUID_LEAF_7_FPUCS (1<<13) // Deprecates FPU CS and FPU DS values if 1.

#endif /* _CPUID__H */

