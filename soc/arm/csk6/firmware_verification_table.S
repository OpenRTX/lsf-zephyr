#include <toolchain.h>

SECTION_FUNC(csk6_private, csk6_image_header)

    /******************** header begin ******************/
    .ascii   "Hr"                                 /*   byte(0~1):   header mark */
    .byte    0x01, 0x00                           /*   byte(2~3):   header version 1.0 */
    .long    0                                    /*   byte(4~7):   image size  */
    .long    0x18000000                           /*   byte(8~11):  "__Vectors", image vma   */
    .byte    0x01, 0x00, 0x00                     /*   byte(12~14): image version 1.0.0 */
    .byte    0x00                                 /*   byte(15~15): image flags */
    .ascii   "VENU", "S_RT", "OS\0\0"             /*   byte(16~27): image name  */
    .short   0x00                                 /*   byte(28~29): patch_offset*/
    .short   0x00                                 /*   byte(30~31): gpt_offset  */
    .space   (7 * 4)                              /*   byte(32~59): reserved    */

    .short   0x0                                  /*   byte(60~61): header check sum */
    .short   0x0                                  /*   byte(62~63): vector check sum */
    /******************** header end ********************/   
