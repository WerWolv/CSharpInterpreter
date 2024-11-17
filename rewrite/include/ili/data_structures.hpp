#pragma once

#include <ili/types.hpp>

#include <array>
#include <span>

namespace ili {

    struct DOSHeader {
        std::array<char, 2> signature;
        u16 extraPageSize;
        u16 numberOfPages;
        u16 relocations;
        u16 headerSizeInParagraphs;
        u16 minimumAllocatedParagraphs;
        u16 maximumAllocatedParagraphs;
        u16 initialSSValue;
        u16 initialRelativeSPValue;
        u16 checksum;
        u16 initialRelativeIPValue;
        u16 initialCSValue;
        u16 relocationsTablePointer;
        u16 overlayNumber;
        std::array<u16, 4> reservedWords;
        u16 oemIdentifier;
        u16 oemInformation;
        std::array<u16, 10> otherReservedWords;
        u32 coffHeaderOffset;

        [[nodiscard]] bool isValid() const;
    };
    static_assert(sizeof(DOSHeader) == 0x40, "dos_header_t size invalid!");

    enum class ArchitectureType : u16 {
        Unknown = 0x00,
        ALPHAAXPOld = 0x183,
        ALPHAAXP = 0x184,
        ALPHAAXP64Bit = 0x284,
        AM33 = 0x1D3,
        AMD64 = 0x8664,
        ARM = 0x1C0,
        ARM64 = 0xAA64,
        ARMNT = 0x1C4,
        CLRPureMSIL = 0xC0EE,
        EBC = 0xEBC,
        I386 = 0x14C,
        I860 = 0x14D,
        IA64 = 0x200,
        LOONGARCH32 = 0x6232,
        LOONGARCH64 = 0x6264,
        M32R = 0x9041,
        MIPS16 = 0x266,
        MIPSFPU = 0x366,
        MIPSFPU16 = 0x466,
        MOTOROLA68000 = 0x268,
        POWERPC = 0x1F0,
        POWERPCFP = 0x1F1,
        POWERPC64 = 0x1F2,
        R3000 = 0x162,
        R4000 = 0x166,
        R10000 = 0x168,
        RISCV32 = 0x5032,
        RISCV64 = 0x5064,
        RISCV128 = 0x5128,
        SH3 = 0x1A2,
        SH3DSP = 0x1A3,
        SH4 = 0x1A6,
        SH5 = 0x1A8,
        THUMB = 0x1C2,
        WCEMIPSV2 = 0x169
    };

    enum class PEFormat : u16 {
        ROM = 0x107,
        PE32 = 0x10B,
        PE32Plus = 0x20B
    };

    enum class SubsystemType : u16 {
        Unknown = 0x00,
        Native = 0x01,
        WindowsGUI = 0x02,
        WindowsCUI = 0x03,
        OS2CUI = 0x05,
        POSIXCUI = 0x07,
        Windows9xNative = 0x08,
        WindowsCEGUI = 0x09,
        EFIApplication = 0x0A,
        EFIBootServiceDriver = 0x0B,
        EFIRuntimeDriver = 0x0C,
        EFIROM = 0x0D,
        Xbox = 0x0E,
        WindowsBootApplication = 0x10
    };

    enum class DataDirectoryType : u8 {
        Export                  = 0,
        Import                  = 1,
        Resource                = 2,
        Exception               = 3,
        Certificate             = 4,
        BaseRelocation          = 5,
        Debug                   = 6,
        ArchitectureData        = 7,
        GlobalPointer           = 8,
        ThreadLocalStorage      = 9,
        LoadConfig              = 10,
        BoundImport             = 11,
        ImportAddress           = 12,
        DelayImportDescriptor   = 13,
        CLRRuntimeHeader        = 14,
        Reserved                = 15
    };

    struct DataDirectory {
        u32 rva;
        u32 size;
    };
    static_assert(sizeof(DataDirectory) == 0x08);

    struct OptionalHeader {
        PEFormat magic;
        u8 majorLinkerVersion;
        u8 minorLinkerVersion;
        u32 sizeOfCode;
        u32 sizeOfInitializedData;
        u32 sizeOfUninitializedData;
        u32 addressOfEntryPoint;
        u32 baseOfCode;
        u64 imageBase;
        u32 virtualSectionAlignment;
        u32 rawSectionAlignment;
        u16 majorOperatingSystemVersion;
        u16 minorOperatingSystemVersion;
        u16 majorImageVersion;
        u16 minorImageVersion;
        u16 majorSubsystemVersion;
        u16 minorSubsystemVersion;
        u32 win32VersionValue;
        u32 sizeOfImage;
        u32 sizeOfHeaders;
        u32 checksum;
        SubsystemType subsystem;
        u16 dllCharacteristics;
        u64 sizeOfStackReserve;
        u64 sizeOfStackCommit;
        u64 sizeOfHeapReserve;
        u64 sizeOfHeapCommit;
        u32 loaderFlags;
        u32 numberOfRVAsAndSizes;

        [[nodiscard]] bool isValid() const;
    };
    static_assert(sizeof(OptionalHeader) == 0x70);

    struct COFFHeader {
        std::array<char, 4> signature;
        ArchitectureType architecture;
        u16 numberOfSections;
        u32 timeDateStamp;
        u32 pointerToSymbolTable;
        u32 numberOfSymbols;
        u16 sizeOfOptionalHeader;
        u16 characteristics;

        [[nodiscard]] bool isValid() const;
    };
    static_assert(sizeof(COFFHeader) == 0x18);

    struct SectionHeader {
        std::array<char, 8> name;

        u32 virtualSize;
        u32 virtualAddress;

        u32 rawDataSize;
        u32 rawDataOffset;
        u32 relocationsOffset;
        u32 lineNumbersOffset;

        u16 numRelocations;
        u16 numLineNumbers;

        u32 characteristics;
    };

    struct Section {
        SectionHeader *header;
        std::span<u8> data;

        [[nodiscard]] u64 offsetFromRva(u64 rva) const {
            return rva - header->virtualAddress;
        }

        [[nodiscard]] u64 executableOffsetFromRva(u64 rva) const {
            return header->rawDataOffset + offsetFromRva(rva);
        }

        [[nodiscard]] u8* bytesFromRva(u64 rva) const {
            return data.data() + offsetFromRva(rva);
        }
    };

    struct CRLRuntimeHeader {
        u32 headerSize;
        u16 runtimeVersionMajor;
        u16 runtimeVersionMinor;
        DataDirectory metaData;
        u32 flags;
        u32 entryPointToken;
        u64 resources;
        u64 strongNameSignature;
        u64 codeManagerTable;
        u64 vTableFixups;
        u64 exportAddressTableJumps;
        u64 managedNativeHeader;

        [[nodiscard]] bool isValid() const;
    };

    struct StreamHeader {
        u32 offset;
        u32 size;
        std::array<char, 32> name;
    };

    struct Metadata {
        std::array<char, 4> signature;
        u16 versionMajor;
        u16 versionMinor;
        u32 reserved;
        u32 length;
        std::array<char, 0xFF> version;
        u16 flags;
        u16 streams;

        [[nodiscard]] bool isValid() const;
    };

    struct StreamTilde {
        u32 reserved1;
        u8 versionMajor;
        u8 versionMinor;
        u8 heapSize;
        u8 reserved2;
        u64 valid;
        u64 sorted;
    };

    enum class CorILMethodType : u8 {
        TinyFormat = 0x02,
        FatFormat  = 0x03
    };

    struct [[gnu::packed]] CorILMethodTiny {
        u8 type : 2;
        u8 size : 6;
    };
    static_assert(sizeof(CorILMethodTiny) == 1);

    struct [[gnu::packed]] CorILMethodFat {
        u16 type : 2;
        u16 flags : 10;
        u16 headerSize : 4;
        u16 maxStack;
        u32 codeSize;
        u32 localVarSigTok;
    };
    static_assert(sizeof(CorILMethodFat) == 12);

    enum class ManagedPointer : u64 {};
    enum class UnmanagedPointer : u64 {};
    enum class NativeInt : i64 {};
    enum class NativeUnsignedInt : u64 {};
    struct Float {
        Float() = default;
        Float(f64 value) : value(value) {}

        operator f32() const {
            return value;
        }

        operator f64() const {
            return value;
        }

        f64 value;
    };

    enum class SignatureElementType : u8 {
        End,
        Void,
        Boolean,
        Char,
        I1,
        U1,
        I2,
        U2,
        I4,
        U4,
        I8,
        U8,
        R4,
        R8,
        String,
        Ptr,
        ByRef,
        ValueType,
        Class,
        Var,
        Array,
        GenericInst,
        TypedByRef,
        I,
        U,
        FuncPtr,
        Object,
        SzArray,
        MVar,
        CmodReqd,
        CmodOpt,
        Internal,
        Modifier,
        Sentinel,
        Pinned
    };

    constexpr u8 getSignatureElementTypeSize(SignatureElementType type) {
        switch (type) {
            using enum SignatureElementType;

            case Boolean:   return 1;
            case Char:      return 2;
            case I1:        return 1;
            case U1:        return 1;
            case I2:        return 2;
            case U2:        return 2;
            case I4:        return 4;
            case U4:        return 4;
            case I8:        return 8;
            case U8:        return 8;
            case R4:        return 4;
            case R8:        return 8;
            case String:    return 8;
            case Ptr:       return 8;
            default:        return 0;
        }
    }

}
