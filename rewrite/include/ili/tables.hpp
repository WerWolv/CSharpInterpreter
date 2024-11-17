#pragma once

#include <ili/types.hpp>
#include <fmt/format.h>

namespace ili::table {

    struct StringIndex {
        u16 index;
    };

    struct UserStringIndex {
        u16 index;
    };

    struct BlobIndex {
        u16 index;
    };

    struct GuidIndex {
        u16 index;
    };

    template<std::unsigned_integral T>
    struct TableIndex {
        T index;

        bool operator==(const TableIndex& other) const = default;
        auto operator<=>(const TableIndex& other) const = default;
    };

    enum class TableID : u8 { };

    struct Token {
        explicit Token() : Token(0x00) { }

        template<typename T = u16>
        explicit Token(TableID id, TableIndex<T> index) : value(u32(id) << 24U | (index.index & 0x00FF'FFFFU)) {}
        explicit Token(u32 value) : value(value) {}

        u32 value;

        [[nodiscard]] TableID getId() const { return TableID(value >> 24U); }
        [[nodiscard]] TableIndex<u32> getIndex() const { return TableIndex(value & 0x00FF'FFFFU); }
    };
    static_assert(sizeof(Token) == 4);

    enum class CodedIndexType : u8 {
        TypeDefOrRef,
        HasConstant,
        HasCustomAttribute,
        HasFieldMarshall,
        HasDeclSecurity,
        MemberRefParent,
        HasSemantics,
        MethodDefOrRef,
        MemberForwarded,
        Implementation,
        CustomAttributeType,
        ResolutionScope,
        TypeOrMethodDef
    };

    Token codedIndexToToken(CodedIndexType type, u8 tag, u32 index);

    template<CodedIndexType Type, typename T = u16>
    struct CodedIndex {
        u16 value;

        [[nodiscard]] static u8 getTagWidth() {
            switch (Type) {
                using enum CodedIndexType;

                case TypeDefOrRef:          return 2;
                case HasConstant:           return 2;
                case HasCustomAttribute:    return 5;
                case HasFieldMarshall:      return 1;
                case HasDeclSecurity:       return 2;
                case MemberRefParent:       return 3;
                case HasSemantics:          return 1;
                case MethodDefOrRef:        return 1;
                case MemberForwarded:       return 1;
                case Implementation:        return 2;
                case CustomAttributeType:   return 3;
                case ResolutionScope:       return 2;
                case TypeOrMethodDef:       return 1;
                default: return 0;
            }
        }

        [[nodiscard]] u8 getTag() const { return u8(value & ((1 << getTagWidth()) - 1)); }
        [[nodiscard]] u16 getIndex() const { return u16(value >> getTagWidth()); }

        [[nodiscard]] operator Token() const {
            return codedIndexToToken(Type, getTag(), getIndex());
        }
    };

    struct TableBase {};

    template<u8 ID_>
    struct Table : TableBase {
        constexpr static auto ID = TableID(ID_);
    };

    struct MethodImplAttributes {
        u8 codeType : 2;
        u8 managed : 1;
        u8 noInlining : 1;
        u8 forwardRef : 1;
        u8 synchronized : 1;
        u8 noOptimization : 1;
        u8 preserveSig : 1;
        u8 padding3 : 4;
        u8 internalCall : 1;
    };
    static_assert(sizeof(MethodImplAttributes) == 2);

    struct MethodAttributes {
        u8 memberAccess : 3;
        u8 unmanagedExport : 1;
        u8 isStatic : 1;
        u8 isFinal : 1;
        u8 isVirtual : 1;
        u8 hideBySig : 1;
        u8 vtableLayout : 1;
        u8 isStrict : 1;
        u8 isAbstract : 1;
        u8 isSpecialName : 1;
        u8 padding1 : 1;
        u8 pinvokeImpl : 1;
    };
    static_assert(sizeof(MethodAttributes) == 2);

    struct MethodDef : Table<0x06> {
        u32 rva;
        MethodImplAttributes implFlags;
        MethodAttributes flags;
        StringIndex nameIndex;
        BlobIndex signatureIndex;
        TableIndex<u16> paramListIndex;
    };

    struct AssemblyFlags {
        u8 publicKey : 1;
        u8 padding1 : 7;
        u8 retargetable : 1;
        u8 padding2 : 5;
        u8 disableJITcompileOptimizer : 1;
        u8 enableJITcompileTracking : 1;
        u16 padding : 16;
    };
    static_assert(sizeof(AssemblyFlags) == 4);

    struct AssemblyRef : Table<0x23> {
        u16 versionMajor;
        u16 versionMinor;
        u16 buildNumber;
        u16 revisionNumber;
        AssemblyFlags flags;
        BlobIndex publicKeyOrTokenIndex;
        StringIndex nameIndex;
        StringIndex cultureIndex;
        BlobIndex hashValueIndex;
    };

    struct TypeRef : Table<0x01> {
        CodedIndex<CodedIndexType::ResolutionScope> resolutionScopeIndex;
        StringIndex typeNameIndex;
        StringIndex typeNamespaceIndex;
    };

    struct TypeSpec : Table<0x1B> {
        BlobIndex signatureIndex;
    };

    struct ParamAttributes {
        u16 in : 1;
        u16 out : 1;
        u16 padding1 : 2;
        u16 optional : 1;
        u16 padding2 : 7;
        u16 hasDefault : 1;
        u16 hasFieldMarshall : 1;
        u16 padding3 : 2;
    };
    static_assert(sizeof(ParamAttributes) == 2);

    struct Param : Table<0x08> {
        ParamAttributes flags;
        u16 sequence;
        StringIndex nameIndex;
    };

    struct PropertyAttributes {
        u16 padding1 : 9;
        u16 specialName : 1;
        u16 rtSpecialName : 1;
        u16 padding2 : 1;
        u16 hasDefault : 1;
        u16 padding3 : 3;
    };
    static_assert(sizeof(PropertyAttributes) == 2);

    struct Property : Table<0x17> {
        PropertyAttributes flags;
        StringIndex nameIndex;
        BlobIndex typeIndex;
    };

    enum class AssemblyHashAlgorithm : u32 {
        None = 0x0000,
        MD5  = 0x8003,
        SHA1 = 0x8004
    };

    struct Assembly : Table<0x20> {
        AssemblyHashAlgorithm hashAlgId;
        u16 majorVersion, minorVersion, buildNumber, revisionNumber;
        AssemblyFlags flags;
        BlobIndex publicKeyIndex;
        StringIndex nameIndex;
    };

    struct ModuleRef : Table<0x1A> {
        StringIndex nameIndex;
    };

    struct FileAttributes {
        u32 containsNoMetaData : 1;
        u32 padding1 : 31;
    };
    static_assert(sizeof(FileAttributes) == 4);

    struct File : Table<0x26> {
        FileAttributes flags;
        StringIndex nameIndex;
        BlobIndex hashValueIndex;
    };

    struct EventAttributes {
        u16 padding1 : 9;
        u16 specialName : 1;
        u16 rtSpecialName : 1;
        u16 padding2 : 5;
    };
    static_assert(sizeof(EventAttributes) == 2);

    struct Event : Table<0x14> {
        EventAttributes eventFlags;
        StringIndex nameIndex;
        CodedIndex<CodedIndexType::TypeDefOrRef> eventType;
    };

    struct TypeAttributes {
        u8 visibility : 3;
        u8 classLayout : 2;
        u8 classSemantics : 1;
        u8 padding1 : 1;
        u8 specialSemantics : 4;
        u8 rtSpecialName : 1;
        u8 implementation : 2;
        u8 padding3 : 2;
        u8 stringFormatting : 2;
        u8 hasSecurity : 1;
        u8 padding4 : 2;
        u8 isTypeForwarder : 1;
        u8 customStringFormat : 2;
        u8 beforeFieldInit : 1;
    };
    static_assert(sizeof(TypeAttributes) == 4);

    struct ExportedType : Table<0x27> {
        TypeAttributes flags;
        TableIndex<u32> typeDefIdHintIndex;
        StringIndex typeNameIndex;
        StringIndex typeNamespaceIndex;
        CodedIndex<CodedIndexType::Implementation> implementation;
    };

    struct InterfaceImpl : Table<0x09> {
        TableIndex<u16> classIndex;
        CodedIndex<CodedIndexType::TypeDefOrRef> interface;
    };

    struct DeclSecurity : Table<0x0E> {
        u16 action;
        CodedIndex<CodedIndexType::HasDeclSecurity> parent;
        BlobIndex permissionSetIndex;
    };

    struct StandAloneSig : Table<0x11> {
        BlobIndex signatureIndex;
    };

    struct ManifestResourceAttribute {
        u32 isPublic : 1;
        u32 isPrivate : 1;
        u32 padding : 30;
    };
    static_assert(sizeof(ManifestResourceAttribute) == 4);

    struct ManifestResource : Table<0x28> {
        u32 offset;
        ManifestResourceAttribute flags;
        StringIndex nameIndex;
        CodedIndex<CodedIndexType::Implementation> implementation;
    };

    struct GenericParamAttributes {
        u16 covariant : 1;
        u16 contravariant : 1;
        u16 referenceTypeConstraint : 1;
        u16 notNullableValueTypeConstraint : 1;
        u16 defaultConstructorConstraint : 1;
        u16 padding1 : 11;
    };
    static_assert(sizeof(GenericParamAttributes) == 2);

    struct GenericParam : Table<0x2A> {
        u16 numberIndex;
        GenericParamAttributes flags;
        CodedIndex<CodedIndexType::MethodDefOrRef> owner;
        StringIndex nameIndex;
    };

    struct GenericParamConstraint : Table<0x2C> {
        TableIndex<u16> ownerIndex;
        CodedIndex<CodedIndexType::TypeDefOrRef> constraint;
    };

    struct MethodSpec : Table<0x2B> {
        CodedIndex<CodedIndexType::MethodDefOrRef> method;
        BlobIndex instantiationIndex;
    };

    struct TypeDef : Table<0x02> {
        u32 flags;
        StringIndex typeNameIndex;
        StringIndex typeNamespaceIndex;
        CodedIndex<CodedIndexType::TypeDefOrRef> extendsIndex;
        TableIndex<u16> fieldListIndex;
        TableIndex<u16> methodListIndex;
    };

    struct MemberRef : Table<0x0A> {
        CodedIndex<CodedIndexType::MemberRefParent> classIndex;
        StringIndex nameIndex;
        BlobIndex signatureIndex;
    };

    struct Module : Table<0x00> {
        u16 generation;
        StringIndex nameIndex;
        GuidIndex mvId;
        GuidIndex encId;
        GuidIndex encBaseId;
    };

    struct ClassLayout : Table<0x0F> {
        u16 packingSize;
        u32 classSize;
        TableIndex<u16> parentIndex;
    };

    struct FieldAttributes {
        u8 fieldAccess : 3;
        u8 padding1 : 1;
        u8 isStatic : 1;
        u8 initOnly : 1;
        u8 literal : 1;
        u8 notSerialized : 1;
        u8 hasFieldRVA : 1;
        u8 specialName : 1;
        u8 rtSpecialName : 1;
        u8 padding3 : 1;
        u8 hasFieldMarshal : 1;
        u8 pinvokeImpl : 1;
        u8 hasDefault : 1;
    };
    static_assert(sizeof(FieldAttributes) == 2);

    struct Field : Table<0x04> {
        FieldAttributes flags;
        StringIndex nameIndex;
        BlobIndex signatureIndex;
    };

    template<typename T>
    concept TableType = std::derived_from<T, TableBase>;

}

template<>
struct fmt::formatter<ili::table::Token> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    constexpr auto format(const auto &token, format_context &ctx) const {
        return fmt::format_to(ctx.out(), "Token 0x{:08X}", token.value);
    }
};