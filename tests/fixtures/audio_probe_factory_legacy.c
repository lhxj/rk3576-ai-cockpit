static int amp_project_source_legacy_script(const image_header_t *hdr,
					    ulong loaded_size)
{
	const u32 *table;
	const char *script;
	u32 data_size, script_size;

	/* Factory mkimage omits -A: its text SCRIPT is tagged PPC/Linux. */
	if (!image_check_magic(hdr) || !image_check_type(hdr, IH_TYPE_SCRIPT) ||
	    !image_check_arch(hdr, IH_ARCH_PPC) || !image_check_os(hdr, IH_OS_LINUX) ||
	    image_get_comp(hdr) != IH_COMP_NONE || !image_check_hcrc(hdr))
		return 1;
	data_size = image_get_size(hdr);
	/* Only the original single, uncompressed, small SCRIPT encoding. */
	if (data_size <= 2 * sizeof(u32) || data_size > 64 * 1024 ||
	    loaded_size != sizeof(*hdr) + (ulong)data_size)
		return 1;
	if (!image_check_dcrc(hdr))
		return 1;
	table = (const u32 *)((const char *)hdr + sizeof(*hdr));
	script_size = uimage_to_cpu(table[0]);
	if (!script_size || table[1] ||
	    script_size != data_size - 2 * sizeof(u32))
		return 1;
	script = (const char *)(table + 2);
	return run_command_list(script, script_size, 0);
}
