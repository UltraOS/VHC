#include "Ext2.h"
#include "Utilities/Common.h"

Ext2::Ext2(DiskImage& image, size_t lba_offset, size_t sector_count, const additional_options_t& options)
	: FileSystem(image, lba_offset, sector_count)
{
	// TODO: unhardcode later
	size_t bytes_per_block = 4096;
	size_t bytes_per_block_group = 128 * MB;
	size_t bytes_per_inode_ratio = 4096;
	
	// TODO: log2(bytes_per_block)
	m_superblock.s_log_block_size = 2;
	m_superblock.s_log_cluster_size = 2;

	m_superblock.s_blocks_count_lo = (sector_count * DiskImage::sector_size) / bytes_per_block;
	m_superblock.s_first_data_block = bytes_per_block == 1024;

	auto free_bytes = m_superblock.s_blocks_count_lo * bytes_per_block;

	m_superblock.s_inodes_count = ceiling_divide(free_bytes, bytes_per_inode_ratio);
	m_superblock.s_free_inodes_count = m_superblock.s_inodes_count;
	m_superblock.s_blocks_per_group = bytes_per_block_group / bytes_per_block;

	auto block_groups = ceiling_divide(m_superblock.s_blocks_count_lo, m_superblock.s_blocks_per_group);
	m_superblock.s_inodes_per_group = ceiling_divide(m_superblock.s_inodes_count, block_groups);

	auto blocks_for_group_descriptors = ceiling_divide(block_groups * sizeof(BlockGroupDescriptor32), bytes_per_block);
	auto blocks_for_inodes = ceiling_divide<size_t>(m_superblock.s_inodes_per_group * m_superblock.s_inode_size, bytes_per_block);

	size_t blocks_left = m_superblock.s_blocks_count_lo;
	size_t block_offset = m_superblock.s_first_data_block + 1;
	size_t block_group_idx = 0;

	m_block_groups.resize(block_groups);
	for (auto& bg : m_block_groups) {
		auto offset_within_group = block_offset;

		bg.block_bitmap.resize(bytes_per_block);
		bg.inode_bitmap.resize(m_superblock.s_inodes_per_group);

		offset_within_group += blocks_for_group_descriptors;

		auto& desc = bg.descriptor;
		desc.bg_block_bitmap_lo = offset_within_group++;
		desc.bg_inode_bitmap_lo = offset_within_group++;
		desc.bg_inode_table_lo = offset_within_group;
		offset_within_group += blocks_for_inodes;

		desc.bg_free_inodes_count_lo = m_superblock.s_inodes_per_group;
		desc.bg_free_blocks_count_lo = m_superblock.s_blocks_per_group - (offset_within_group - block_offset);

		mark_as_allocated(block_group_idx++, 0, offset_within_group, Type::BLOCK);
	}

	// reserve first 10 inodes
	mark_as_allocated(0, 0, 11, Type::INODE);

	// Revision 0 means these are hardcoded
	m_superblock.s_inode_size = 128;
	m_superblock.s_first_ino = 11;
}

void Ext2::store(const FSObject&)
{

}

void Ext2::mark_as_allocated(size_t block_group, size_t begin, size_t end, Type type)
{
	auto& bg = m_block_groups.at(block_group);

	auto& bitmap = type == Type::INODE ? bg.inode_bitmap : bg.block_bitmap;

	for (size_t i = begin; i < end; ++i) {
		auto byte_offset = begin / 8;
		auto bit_offset = begin % 8;
	
		if ((bit_offset == 0) && ((end - i) > 8)) {
			bitmap.at(byte_offset) = 0xFF;
			i += 7;
			continue;
		}
		 
		bitmap.at(byte_offset) |= 1 << bit_offset;
	}
}

Ext2::~Ext2()
{
}
