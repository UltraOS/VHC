#pragma once

#include "FileSystems/FileSystem.h"
#include "FileSystems/Ext/Structures.h"

class Ext2 final : public FileSystem {
public:
	Ext2(DiskImage& image, size_t lba_offset, size_t sector_count, const additional_options_t& options);

	void store(const FSObject&) override;

	~Ext2();

private:
	enum class Type {
		INODE,
		BLOCK
	};
	void mark_as_allocated(size_t block_group, size_t begin, size_t end, Type);


private:
	Superblock m_superblock {};

	struct BlockGroup {
		BlockGroupDescriptor32 descriptor {};
		std::vector<uint8_t> inode_bitmap;
		std::vector<uint8_t> block_bitmap;
	};

	std::vector<BlockGroup> m_block_groups;
};