#include <codecvt>
#include <ili/assembly.hpp>
#include <ili/utils.hpp>

#include <wolv/io/file.hpp>
#include <ranges>
#include <utility>
#include <cstring>

#include <wolv/utils/string.hpp>
#include <fmt/format.h>

namespace ili {

    Assembly::Assembly(const std::filesystem::path& path) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (!file.isValid())
            throw std::invalid_argument("File could not be opened");

        m_data = file.readVector();
        parse();
    }

    Assembly::Assembly(std::span<const u8> data) : m_data(data.begin(), data.end()) {
        parse();
    }

    void Assembly::parseHeaders() {
        if (m_data.empty()) {
            throw std::invalid_argument("No data provided");
        }

        // Parse DOS Header
        {
            m_dosHeader = new(m_parsePointer) DOSHeader;
            if (!m_dosHeader->isValid()) {
                throw std::invalid_argument("Invalid DOS header");
            }
            m_parsePointer = m_data.data() + m_dosHeader->coffHeaderOffset;
        }

        // Parse COFF Header
        {
            m_coffHeader = new(m_parsePointer) COFFHeader;
            if (!m_coffHeader->isValid()) {
                throw std::invalid_argument("Invalid COFF header");
            }
            m_parsePointer += sizeof(COFFHeader);
        }

        // Parse Optional Header
        {
            m_optionalHeader = new(m_parsePointer) OptionalHeader;
            if (!m_optionalHeader->isValid()) {
                throw std::invalid_argument("Invalid Optional header");
            }
            m_parsePointer += sizeof(OptionalHeader);
        }
    }

    void Assembly::parseSections() {
        // Parse Data directories
        {
            m_directories = {
                reinterpret_cast<DataDirectory*>(m_parsePointer),
                m_optionalHeader->numberOfRVAsAndSizes
            };

            for (auto &directory : m_directories) {
                directory = *new(m_parsePointer) DataDirectory;
                m_parsePointer += sizeof(DataDirectory);
            }
        }

        // Parse Section headers and extract section data
        {
            m_sections.resize(m_coffHeader->numberOfSections);
            for (auto [section, directory] : std::views::zip(m_sections, m_directories)) {
                section.header = new(m_parsePointer) SectionHeader;
                m_parsePointer += sizeof(SectionHeader);

                section.data = {
                    m_data.data() + section.executableOffsetFromRva(section.header->virtualAddress),
                    section.header->virtualSize
                };
            }
        }
    }

    void Assembly::parseStreamHeaders() {
        // Parse CRL Runtime Header
        {
            const auto &crlDirectory = m_directories[std::to_underlying(DataDirectoryType::CLRRuntimeHeader)];
            const auto crlSection = getVirtualSection(crlDirectory.rva);
            if (crlSection == nullptr) {
                throw std::invalid_argument("Cannot find CRL Runtime Header section");
            }

            m_parsePointer = m_data.data() + crlSection->executableOffsetFromRva(crlDirectory.rva),
            m_crlRuntimeHeader = new(m_parsePointer) CRLRuntimeHeader;

            if (!m_crlRuntimeHeader->isValid()) {
                throw std::invalid_argument("Invalid CRL Runtime header");
            }
        }

        // Parse Metadata section
        {
            const auto metadataSection = getVirtualSection(m_crlRuntimeHeader->metaData.rva);
            if (metadataSection == nullptr) {
                throw std::invalid_argument("Cannot find Metadata section");
            }

            m_parsePointer = m_data.data() + metadataSection->executableOffsetFromRva(m_crlRuntimeHeader->metaData.rva),
            m_metadataPointer = m_parsePointer;
            {
                m_metadata = {};

                // Parse first part
                std::memmove(&this->m_metadata, m_parsePointer, offsetof(Metadata, version));
                m_parsePointer += offsetof(Metadata, version);

                // Parse variable length version field
                std::memmove(&this->m_metadata.version, m_parsePointer, this->m_metadata.length);
                m_parsePointer += this->m_metadata.length;

                // Parse remaining part
                std::memmove(&this->m_metadata.flags, m_parsePointer, 2 * sizeof(u16));
                m_parsePointer += 2 * sizeof(u16);

                if (!m_metadata.isValid()) {
                    throw std::invalid_argument("Invalid Metadata section");
                }
            }
        }

        // Parse Stream Headers
        {
            for (u16 stream = 0; stream < this->m_metadata.streams; stream += 1) {
                StreamHeader header = {};
                std::memmove(&header, m_parsePointer, sizeof(StreamHeader::offset) + sizeof(StreamHeader::size));
                m_parsePointer += sizeof(StreamHeader::offset) + sizeof(StreamHeader::size);

                const auto nameSize = wolv::util::strnlen(reinterpret_cast<const char*>(m_parsePointer), 32) + 1;
                std::memmove(header.name.data(), m_parsePointer, nameSize);

                m_parsePointer += util::alignUp<u64>(nameSize, 4);
                this->m_streamHeaders.push_back(header);
            }
        }
    }

    static constexpr u8 getMetadataTableSize(std::size_t index) {
        // TODO: Some of these values depend on if a table/heap has more than 2^16 entries
        // TODO: For now, assume we don't reach that limit
        constexpr static std::array<u8, 64> MetadataTableSizes = {
            0x0A, 0x06, 0x0E, 0x00, 0x06, 0x00, 0x0E, 0x00,
            0x06, 0x00, 0x06, 0x00, 0x06, 0x00, 0x00, 0x00,
            0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x16, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        if (index >= MetadataTableSizes.size())
            return 0;

        return MetadataTableSizes[index];
    }

    void Assembly::parseStreams() {
        parseStreamHeaders();

        for (const auto &streamHeader : m_streamHeaders) {
            const auto streamName = std::string_view(streamHeader.name.data());
            if (streamName == "#~") {
                const auto *tildeStream = new(m_metadataPointer + streamHeader.offset) StreamTilde;

                // Skip to rows array
                m_parsePointer += 24;

                m_streams.userString.largeIndices = false;
                m_streams.string.largeIndices = !!(tildeStream->heapSize & 0x01);
                m_streams.guid.largeIndices   = !!(tildeStream->heapSize & 0x02);
                m_streams.blob.largeIndices   = !!(tildeStream->heapSize & 0x04);

                std::array<u64, 64> tableRowCounts = {};
                for (u8 i = 0; i < 64; i++) {
                    if ((tildeStream->valid & (1ULL << i)) == (1ULL << i)) {
                        tableRowCounts[i] = *reinterpret_cast<u32*>(m_parsePointer);
                        m_parsePointer += sizeof(u32);
                    }
                }

                u32 index = 0;
                for (const auto &count : tableRowCounts) {
                    auto &table = m_streams.tilde.tableData[index];
                    if (count != 0) {
                        u8 tableSize = getMetadataTableSize(index);

                        table.resize(count);
                        for (auto &tableEntry : table) {
                            tableEntry = { m_parsePointer, tableSize };
                            m_parsePointer += tableSize;
                        }
                    }

                    index += 1;
                }
            } else if (streamName == "#Strings") {
                m_streams.string.data = {
                    m_metadataPointer + streamHeader.offset,
                    streamHeader.size
                };
            } else if (streamName == "#US") {
                m_streams.userString.data = {
                    m_metadataPointer + streamHeader.offset,
                    streamHeader.size
                };
            } else if (streamName == "#Blob") {
                m_streams.blob.data = {
                    m_metadataPointer + streamHeader.offset,
                    streamHeader.size
                };
            } else if (streamName == "#GUID") {
                m_streams.guid.data = {
                    m_metadataPointer + streamHeader.offset,
                    streamHeader.size
                };
            }
        }
    }

    const table::TypeDef* Assembly::getTypeDefOfMethod(const table::MethodDef *methodToFind) const {
        if (methodToFind == nullptr)
            return nullptr;

        const auto methodDefCount = getTableRowCount(table::MethodDef::ID);
        for (u64 i = 1; i < methodDefCount; i++) {
            const auto currTypeDef = getTableEntryByIndex<table::TypeDef>(table::TableIndex(i + 0));
            const auto nextTypeDef = getTableEntryByIndex<table::TypeDef>(table::TableIndex(i + 1));

            const auto currMethodDef = getTableEntryByIndex<table::MethodDef>(currTypeDef->methodListIndex);
            const auto nextMethodDef = getTableEntryByIndex<table::MethodDef>(nextTypeDef->methodListIndex);

            if (methodToFind >= currMethodDef && methodToFind < nextMethodDef)
                return nextTypeDef;
        }

        return getTableEntryByIndex<table::TypeDef>(table::TableIndex(methodDefCount));
    }

    const table::ClassLayout* Assembly::getClassLayoutOfType(const table::TypeDef *typeDef) const {
        if (typeDef == nullptr)
            return nullptr;

        for (u32 i = 0; i < getTableRowCount(table::ClassLayout::ID); i++) {
            const auto currClassLayout = getTableEntryByIndex<table::ClassLayout>(table::TableIndex(i));

            const auto *parentTypeDef = getTableEntryByIndex<table::TypeDef>(currClassLayout->parentIndex);
            if (parentTypeDef == typeDef)
                return currClassLayout;
        }

        return nullptr;
    }

    void Assembly::parseMethods() {

    }

    void Assembly::parse() {
        m_parsePointer = m_data.data();

        parseHeaders();
        parseSections();
        parseStreams();
        parseMethods();
    }

    const Section* Assembly::getVirtualSection(u64 rva) const {
        for (auto [section, directory] : std::views::zip(m_sections, m_directories)) {
            if (rva >= section.header->virtualAddress
                && rva <  section.header->virtualAddress + section.header->virtualSize)
                return &section;
        }

        return nullptr;
    }

    table::Token Assembly::getEntrypointMethodToken() const {
        return table::Token(m_crlRuntimeHeader->entryPointToken);
    }

    std::span<const u8> Assembly::getSectionBytes(const Section& section, u64 rva, std::size_t size) const {
        return { section.bytesFromRva(rva), size };
    }

    u64 Assembly::getStackSize() const {
        return m_optionalHeader->sizeOfStackReserve;
    }

    std::string_view Assembly::getString(table::StringIndex index) const {
        return reinterpret_cast<const char *>(&m_streams.string.data[index.index]);
    }

    std::size_t getBlobHeaderSize(u8 firstByte) {
        if ((firstByte & 0x80) == 0x00)
            return 1;
        if ((firstByte & 0xC0) == 0x80)
            return 2;
        if ((firstByte & 0xE0) == 0xC0)
            return 4;

        return 0;
    }

    std::size_t getBlobSize(const u8 *blobStart, u32 index) {
        switch (getBlobHeaderSize(blobStart[index])) {
            case 1: return blobStart[index];
            case 2: return ((blobStart[index] & 0x3F) << 8) + blobStart[index + 1];
            case 4: return ((blobStart[index] & 0x1F) << 24)
                            + (blobStart[index + 1] << 16)
                            + (blobStart[index + 2] <<  8)
                            + (blobStart[index + 3] <<  0);
            default: return 0;
        }
    }

    std::span<const u8> getBlob(const u8 *blobStart, u32 index) {
        const auto blobHeaderSize = getBlobHeaderSize(blobStart[index]);
        const auto blobSize       = getBlobSize(blobStart, index);

        return { &blobStart[index + blobHeaderSize], blobSize };
    }

    std::string Assembly::getUserString(table::UserStringIndex index) const {
        const auto blob = getBlob(m_streams.userString.data.data(), index.index);

        std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conversion;
        return conversion.to_bytes(
            reinterpret_cast<const char16_t*>(blob.data()),
            reinterpret_cast<const char16_t*>(blob.data() + blob.size_bytes() - 1)
        );
    }



    Assembly::QualifiedName Assembly::getQualifiedMemberName(table::Token memberRefToken) const {
        const auto memberRef   = this->getTableEntry<table::MemberRef>(memberRefToken);
        const auto typeRef     = this->getTableEntry<table::TypeRef>(memberRef->classIndex);
        const auto assemblyRef = this->getTableEntry<table::AssemblyRef>(typeRef->resolutionScopeIndex);

        const auto assembly  = this->getString(assemblyRef->nameIndex);
        const auto nameSpace = this->getString(typeRef->typeNamespaceIndex);
        const auto type      = this->getString(typeRef->typeNameIndex);
        const auto method    = this->getString(memberRef->nameIndex);

        return QualifiedName {
            assembly,
            nameSpace,
            type,
            method
        };
    }

    [[nodiscard]] u64 Assembly::getTableRowCount(table::TableID id) const {
        return m_streams.tilde.tableData[u8(id)].size();
    }

    [[nodiscard]] const table::Module* Assembly::getModule() const {
        return getTableEntryByIndex<table::Module>(table::TableIndex<u32>(1));
    }

    const table::MethodDef* Assembly::getMethodByName(std::string_view namespaceName, std::string_view typeName, std::string_view methodName) const {
        // Loop through the MethodDef table to find a method with the name in methodName
        for (u32 index = 0; index < getTableRowCount(table::TypeDef::ID); index += 1) {
            auto typeDef = getTableEntryByIndex<table::TypeDef>(table::TableIndex(index + 1));

            const auto foundTypeNamespaceName = getString(typeDef->typeNamespaceIndex);
            if (namespaceName != foundTypeNamespaceName)
                continue;

            const auto foundTypeName = getString(typeDef->typeNameIndex);
            if (foundTypeName != typeName)
                continue;

            auto nextTypeDef = getTableEntryByIndex<table::TypeDef>(table::TableIndex(index + 2));

            for (u32 methodListIndex = typeDef->methodListIndex.index; index < nextTypeDef->methodListIndex.index; methodListIndex += 1) {
                const auto methodDef = getTableEntryByIndex<table::MethodDef>(table::TableIndex(methodListIndex));
                const auto foundMethodName = getString(methodDef->nameIndex);

                if (foundMethodName != methodName)
                    continue;

                return methodDef;
            }
        }

        return nullptr;
    }

}
