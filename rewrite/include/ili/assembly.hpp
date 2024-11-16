#pragma once

#include <filesystem>
#include <map>
#include <vector>

#include <ili/types.hpp>
#include <ili/data_structures.hpp>
#include <ili/tables.hpp>

namespace ili {

    class Assembly {
    public:
        Assembly() = default;
        explicit Assembly(const std::filesystem::path &path);
        explicit Assembly(std::span<const u8> data);

        [[nodiscard]] const Section* getVirtualSection(u64 rva) const;

        [[nodiscard]] table::Token getEntrypointMethodToken() const;

        [[nodiscard]] std::span<const u8> getUserStringBytes(u32 index) const;
        [[nodiscard]] std::span<const u8> getBlobBytes(u32 index) const;
        [[nodiscard]] std::span<const u8> getSectionBytes(const Section& section, u64 rva, std::size_t size) const;

        [[nodiscard]] std::string_view getString(table::StringIndex index) const;
        [[nodiscard]] std::string getUserString(table::UserStringIndex index) const;

        struct QualifiedName {
            std::string_view assemblyName;
            std::string_view namespaceName;
            std::string_view typeName;
            std::string_view methodName;

            [[nodiscard]] explicit operator std::string() const {
                return fmt::format("[{}]{}.{}::{}", assemblyName, namespaceName, typeName, methodName);
            }

            bool operator<(const QualifiedName& other) const {
                return assemblyName < other.assemblyName &&
                       namespaceName < other.namespaceName &&
                       typeName < other.typeName &&
                       methodName < other.methodName;
            }
        };

        const table::TypeDef* getTypeDefOfMethod(const table::MethodDef *methodToFind) const;
        const table::ClassLayout* getClassLayoutOfType(const table::TypeDef *typeDef) const;

        [[nodiscard]] QualifiedName getQualifiedMemberName(table::Token memberRefToken) const;

        [[nodiscard]] u64 getStackSize() const;

        template<table::TableType T>
        [[nodiscard]] const T* getTableEntry(table::Token token) const {
            if (token.getId() != T::ID) [[unlikely]]
                return nullptr;

            const auto id = u8(token.getId());
            const auto index = token.getIndex().index - 1;

            if (id > m_streams.tilde.tableData.size()) [[unlikely]]
                return nullptr;

            const auto &table = m_streams.tilde.tableData[id];
            if (index > table.size()) [[unlikely]]
                return nullptr;

            return reinterpret_cast<T*>(table[index].data());
        }

        template<table::TableType T, typename IndexType>
        [[nodiscard]] const T* getTableEntryByIndex(table::TableIndex<IndexType> index) const {
            if (index.index == 0 || index.index > m_streams.tilde.tableData[u8(T::ID)].size() + 1) [[unlikely]] {
                return nullptr;
            }

            const auto token = table::Token(T::ID, index);
            return getTableEntry<T>(token);
        }

        [[nodiscard]] u64 getTableRowCount(table::TableID id) const;
        [[nodiscard]] const table::Module* getModule() const;

        template<table::TableType T>
        [[nodiscard]] std::vector<const T*> getAllTableEntries() const {
            std::vector<const T*> result;
            for (u32 index = 0; index < getTableRowCount(T::ID); index += 1) {
                result.push_back(getTableEntryByIndex<T>(table::TableIndex(index + 1)));
            }

            return result;
        }

        [[nodiscard]] const table::MethodDef* getMethodByName(std::string_view namespaceName, std::string_view typeName, std::string_view methodName) const;

        template<table::TableType T>
        [[nodiscard]] table::Token getTokenOfTableEntry(const T *tableEntry) const {
            for (u32 index = 0; index < getTableRowCount(T::ID); index += 1) {
                if (getTableEntryByIndex<T>(table::TableIndex(index + 1)) == tableEntry)
                    return table::Token(T::ID, table::TableIndex(index + 1));
            }

            return table::Token(table::TableID(0xFF), table::TableIndex<u16>(0x00));
        }

    private:
        void parse();
        void parseHeaders();
        void parseSections();
        void parseStreamHeaders();
        void parseStreams();
        void parseMethods();

    private:
        std::vector<u8> m_data;
        u8 *m_parsePointer = nullptr;

        DOSHeader *m_dosHeader = nullptr;
        COFFHeader *m_coffHeader = nullptr;
        std::span<DataDirectory> m_directories;
        std::vector<Section> m_sections;
        OptionalHeader *m_optionalHeader = nullptr;
        CRLRuntimeHeader *m_crlRuntimeHeader = nullptr;
        Metadata m_metadata = { };
        u8 *m_metadataPointer = nullptr;
        std::vector<StreamHeader> m_streamHeaders;
        std::map<QualifiedName, const table::MethodDef*> m_methods;

        struct {
            struct {
                std::array<std::vector<std::span<u8>>, 64> tableData;
            } tilde;
            struct {
                std::span<const u8> data;
                bool largeIndices;
            } string, userString, guid, blob;
        } m_streams;
    };

}
