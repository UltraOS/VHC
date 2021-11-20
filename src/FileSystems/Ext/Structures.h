#pragma once

#include <cstdint>

struct Superblock {
	uint32_t s_inodes_count;
	uint32_t s_blocks_count_lo;
	uint32_t s_r_blocks_count_lo;
	uint32_t s_free_blocks_count_lo;
	uint32_t s_free_inodes_count;
	uint32_t s_first_data_block;
	uint32_t s_log_block_size;
	uint32_t s_log_cluster_size;
	uint32_t s_blocks_per_group;
	uint32_t s_clusters_per_group;
	uint32_t s_inodes_per_group;
	uint32_t s_mtime;
	uint32_t s_wtime;
	uint16_t s_mnt_count;
	uint16_t s_max_mnt_count;
	uint16_t s_magic;
	uint16_t s_state;
	uint16_t s_errors;
	uint16_t s_minor_rev_level;
	uint32_t s_lastcheck;
	uint32_t s_checkinterval;
	uint32_t s_creator_os;
	uint32_t s_rev_level;
	uint16_t s_def_resuid;
	uint16_t s_def_resgid;

	// Everything below this is optional for EXT2
	// EXT2 revision dependant vvvv
	uint32_t s_first_ino;
	uint16_t s_inode_size;
	uint16_t s_block_group_nr;
	uint32_t s_feature_compat;
	uint32_t s_feature_incompat;
	uint32_t s_feature_ro_compat;
	uint8_t s_uuid[16];
	char s_volume_name[16];
	char s_last_mounted[64];
	uint32_t s_algorithm_usage_bitmap;
	// -------------------------------

	// EXT2 performance hints vvvv
	uint8_t s_prealloc_blocks;
	uint8_t s_prealloc_dir_blocks;
	// ---------------------------
	
	uint16_t s_reserved_gdt_blocks;

	// EXT2 journaling support vvvv
	uint8_t s_journal_uuid[16];
	uint32_t s_journal_inum;
	uint32_t s_journal_dev;
	uint32_t s_last_orphan;
	// ----------------------------

	// EXT2 directory indexing support vvvv
	uint32_t s_hash_seed[4];
	uint8_t s_def_hash_version;
	// ------------------------------------

	uint8_t s_jnl_backup_type;
	uint16_t s_desc_size;
	uint32_t s_default_mount_opts;
	uint32_t s_first_meta_bg;
	uint32_t s_mkfs_time;
	uint32_t s_jnl_blocks[17];
	uint32_t s_blocks_count_hi;
	uint32_t s_r_blocks_count_hi;
	uint32_t s_free_blocks_count_hi;
	uint16_t s_min_extra_isize;
	uint16_t s_want_extra_isize;
	uint32_t s_flags;
	uint16_t s_raid_stride;
	uint16_t s_mmp_interval;
	uint64_t s_mmp_block;
	uint32_t s_raid_stripe_width;
	uint8_t s_log_groups_per_flex;
	uint8_t s_checksum_type;
	uint16_t s_reserved_pad;
	uint64_t s_kbytes_written;
	uint32_t s_snapshot_inum;
	uint32_t s_snapshot_id;
	uint64_t s_snapshot_r_blocks_count;
	uint32_t s_snapshot_list;
	uint32_t s_error_count;
	uint32_t s_first_error_time;
	uint32_t s_first_error_ino;
	uint64_t s_first_error_block;
	uint8_t s_first_error_func[32];
	uint32_t s_first_error_line;
	uint32_t s_last_error_time;
	uint32_t s_last_error_ino;
	uint32_t s_last_error_line;
	uint64_t s_last_error_block;
	uint8_t s_last_error_func[32];
	uint8_t s_mount_opts[64];
	uint32_t s_usr_quota_inum;
	uint32_t s_grp_quota_inum;
	uint32_t s_overhead_blocks;
	uint32_t s_backup_bgs[2];
	uint8_t s_encrypt_algos[4];
	uint8_t s_encrypt_pw_salt[16];
	uint32_t s_lpf_ino;
	uint32_t s_prj_quota_inum;
	uint32_t s_checksum_seed;
	uint8_t s_wtime_hi;
	uint8_t s_mtime_hi;
	uint8_t s_mkfs_time_hi;
	uint8_t s_lastcheck_hi;
	uint8_t s_first_error_time_hi;
	uint8_t s_last_error_time_hi;
	uint8_t s_pad[2];
	uint16_t s_encoding;
	uint16_t s_encoding_flags;
	uint32_t s_orphan_file_inum;
	uint32_t s_reserved[94];
	uint32_t s_checksum;
};

struct BlockGroupDescritor32 {
	uint32_t bg_block_bitmap_lo;
	uint32_t bg_inode_bitmap_lo;
	uint32_t bg_inode_table_lo;
	uint16_t bg_free_blocks_count_lo;
	uint16_t bg_free_inodes_count_lo;
	uint16_t bg_used_dirs_count_lo;
	uint16_t bg_flags;
	uint32_t bg_exclude_bitmap_lo;
	uint16_t bg_block_bitmap_csum_lo;
	uint16_t bg_inode_bitmap_csum_lo;
	uint16_t bg_itable_unused_lo;
	uint16_t bg_checksum;
};

struct BlockGroupDescriptor64 : BlockGroupDescritor32 {
	uint32_t bg_block_bitmap_hi;
	uint32_t bg_inode_bitmap_hi;
	uint32_t bg_inode_table_hi;
	uint16_t bg_free_blocks_count_hi;
	uint16_t bg_free_inodes_count_hi;
	uint16_t bg_used_dirs_count_hi;
	uint16_t bg_itable_unused_hi;
	uint32_t bg_exclude_bitmap_hi;
	uint16_t bg_block_bitmap_csum_hi;
	uint16_t bg_inode_bitmap_csum_hi;
	uint32_t bg_reserved;
};

struct Inode {
	uint16_t i_mode;
	uint16_t i_uid;
	uint32_t i_size_lo;
	uint32_t i_atime;
	uint32_t i_ctime;
	uint32_t i_mtime;
	uint32_t i_dtime;
	uint16_t i_gid;
	uint16_t i_links_count;
	uint32_t i_blocks_lo;
	uint32_t i_flags;
	uint32_t i_osd1;

	// Means something else in EXT4 and for special files
	uint32_t i_block[15];
	uint32_t i_generation;
	uint32_t i_file_acl_lo;

	union {
		// EXT4
		uint32_t i_size_high;
		
		// EXT2/3
		uint32_t i_dir_acl;
	};

	uint32_t i_obso_faddr;
	uint32_t i_osd2[3];

	// End of 128 byte inode
	uint16_t i_extra_isize;
	uint16_t i_checksum_hi;
	uint32_t i_ctime_extra;
	uint32_t i_mtime_extra;
	uint32_t i_atime_extra;
	uint32_t i_crtime;
	uint32_t i_crtime_extra;
	uint32_t i_version_hi;
	uint32_t i_projid;
};

struct DirectoryEntry {
	uint32_t inode;
	uint16_t rec_len;
	uint8_t name_len;
	uint8_t file_type;
	char name[255];
};

static_assert(sizeof(Superblock) == 1024, "Incorrect size of ext superblock");
static_assert(sizeof(BlockGroupDescritor32) == 32, "Incorrect size of block group descriptor (32 byte)");
static_assert(sizeof(BlockGroupDescriptor64) == 64, "Incorrect size of block group descriptor (64 byte)");
static_assert(sizeof(Inode) == 160, "Incorrect size of Inode");
