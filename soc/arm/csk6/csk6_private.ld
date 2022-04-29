
SECTION_PROLOGUE(csk6_private,,)
{

/* Located in generated directory. This file is populated by calling
 * zephyr_linker_sources(ROM_START ...). This typically contains the vector
 * table and debug information.
 * #include <snippets-rom-start.ld>
 */
    _vectors_csk6_start = .;
    KEEP(*(.csk6_private))
    KEEP(*(".csk6_private.*"))
    _vectors_csk6_end = .;	

} GROUP_ROM_LINK_IN(RAMABLE_REGION, ROMABLE_REGION)
