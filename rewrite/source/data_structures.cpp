#include <ili/data_structures.hpp>
#include <ili/tables.hpp>

#include <algorithm>

namespace ili {

    namespace table {

        Token codedIndexToToken(CodedIndexType type, u8 tag, u32 index) {
            constexpr static auto InvalidId = TableID(0xFF);
            const auto id = [type, tag] {
                switch (type) {
                    using enum CodedIndexType;
                    case TypeDefOrRef:
                        switch (tag) {
                            case 0: return TypeDef::ID;
                            case 1: return TypeRef::ID;
                            case 2: return TypeSpec::ID;
                            default: return InvalidId;
                        }
                    case HasConstant:
                        switch (tag) {
                            case 0: return Field::ID;
                            case 1: return Param::ID;
                            case 2: return Property::ID;
                            default: return InvalidId;
                        }
                    case HasCustomAttribute:
                        switch (tag) {
                            case 0:  return MethodDef::ID;
                            case 1:  return Field::ID;
                            case 2:  return TypeRef::ID;
                            case 3:  return TypeDef::ID;
                            case 4:  return Param::ID;
                            case 5:  return InterfaceImpl::ID;
                            case 6:  return MemberRef::ID;
                            case 7:  return Module::ID;
                            case 8:  return DeclSecurity::ID;
                            case 9:  return Property::ID;
                            case 10: return Event::ID;
                            case 11: return StandAloneSig::ID;
                            case 12: return ModuleRef::ID;
                            case 13: return TypeSpec::ID;
                            case 14: return Assembly::ID;
                            case 15: return AssemblyRef::ID;
                            case 16: return File::ID;
                            case 17: return ExportedType::ID;
                            case 18: return ManifestResource::ID;
                            case 19: return GenericParam::ID;
                            case 20: return GenericParamConstraint::ID;
                            case 21: return MethodSpec::ID;
                            default: return InvalidId;
                        }
                    case HasFieldMarshall:
                        switch (tag) {
                            case 0: return Field::ID;
                            case 1: return Param::ID;
                            default: return InvalidId;
                        }
                    case HasDeclSecurity:
                        switch (tag) {
                            case 0: return TypeDef::ID;
                            case 1: return MethodDef::ID;
                            case 2: return Assembly::ID;
                            default: return InvalidId;
                        }
                    case MemberRefParent:
                        switch (tag) {
                            case 0: return TypeDef::ID;
                            case 1: return TypeRef::ID;
                            case 2: return ModuleRef::ID;
                            case 3: return MethodDef::ID;
                            case 4: return TypeSpec::ID;
                            default: return InvalidId;
                        }
                    case HasSemantics:
                        switch (tag) {
                            case 0: return Event::ID;
                            case 1: return Property::ID;
                            default: return InvalidId;
                        }
                    case MethodDefOrRef:
                        switch (tag) {
                            case 0: return MethodDef::ID;
                            case 1: return MemberRef::ID;
                            default: return InvalidId;
                        }
                    case MemberForwarded:
                        switch (tag) {
                            case 0: return Field::ID;
                            case 1: return MethodDef::ID;
                            default: return InvalidId;
                        }
                    case Implementation:
                        switch (tag) {
                            case 0: return File::ID;
                            case 1: return AssemblyRef::ID;
                            case 2: return ExportedType::ID;
                            default: return InvalidId;
                        }
                    case CustomAttributeType:
                        switch (tag) {
                            case 3: return MemberRef::ID;
                            default: return InvalidId;
                        }
                    case ResolutionScope:
                        switch (tag) {
                            case 0: return Module::ID;
                            case 1: return ModuleRef::ID;
                            case 2: return AssemblyRef::ID;
                            case 3: return TypeRef::ID;
                            default: return InvalidId;
                        }
                    case TypeOrMethodDef:
                        switch (tag) {
                            case 0: return TypeDef::ID;
                            case 1: return MethodDef::ID;
                            default: return InvalidId;
                        }
                }

                return InvalidId;
            }();

            return Token(id, TableIndex(index));
        }

    }

    bool DOSHeader::isValid() const {
        constexpr static std::array ExpectedSignature = { 'M', 'Z' };

        return std::ranges::equal(this->signature, ExpectedSignature);
    }

    bool COFFHeader::isValid() const {
        constexpr static std::array ExpectedSignature = { 'P', 'E', '\x00', '\x00' };

        return std::ranges::equal(this->signature, ExpectedSignature);
    }

    bool OptionalHeader::isValid() const {
        return this->magic == PEFormat::PE32Plus;
    }

    bool Metadata::isValid() const {
        constexpr static std::array ExpectedSignature = { 'B', 'S', 'J', 'B' };

        return std::ranges::equal(this->signature, ExpectedSignature);
    }

    bool CRLRuntimeHeader::isValid() const {
        return this->headerSize == sizeof(*this);
    }



}