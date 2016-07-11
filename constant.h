#ifndef CONSTANT_ITCT_MPEG_ENCODER
#define CONSTANT_ITCT_MPEG_ENCODER
#define xadd3(xa,xb,xc,xd,h) p=xa+xb, n=xa-xb, xa=p+xc+h, xb=n+xd+h, xc=p-xc+h, xd=n-xd+h // triple-butterfly-add (and possible rounding)
#define xmul(xa,xb,k1,k2,sh) n=k1*(xa+xb), p=xa, xa=(n+(k2-k1)*xb)>>sh, xb=(n-(k2+k1)*p)>>sh // butterfly-mul equ.(2)
using namespace std;
//typedef part
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef struct video video;
typedef struct node node;
typedef struct picture picture;
typedef struct slice slice;
typedef struct macroblock macroblock;
typedef struct gop gop;
typedef struct MCU mcu;
typedef struct pel pel;
typedef struct pixel pixel;
typedef struct frame framme;

const double pi = 3.14159265359;
const double nature_e = 2.71828182846;
const int picture_start_code = 0x100;
const int slice_start_codes_min = 0x101;
const int slice_start_codes_max = 0x1AF;
const int user_start_code = 0x1B2;
const int sequence_header_code = 0x1B3;
const int sequence_error_code = 0x1B4;
const int extension_start_code = 0x1B5;
const int sequence_end_code = 0x1B7;
const int group_start_code = 0x1B8;
const int system_start_code_min = 0x1B9;
const int system_start_code_max = 0x1FF;
const uint8 I_type_picture = 1;
const uint8 P_type_picture = 2;
const uint8 B_type_picture = 3;
const uint8 D_type_picture = 4;
static int fill1[] = {0, 1, 3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535};
static int Rowindex[64]={0,0,1,2,1,0,0,1,2,3,4,3,2,1,0,0,1,2,3,4,5,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,7,6,5,4,3,2,1,2,3,4,5,6,7,7,6,5,4,3,4,5,6,7,7,6,5,6,7,7};
static int Colindex[64]={0,1,0,0,1,2,3,2,1,0,0,1,2,3,4,5,4,3,2,1,0,0,1,2,3,4,5,6,7,6,5,4,3,2,1,0,1,2,3,4,5,6,7,7,6,5,4,3,2,3,4,5,6,7,7,6,5,4,5,6,7,7,6,7};
const double pel_aspect_ratio_table[16] = {-1, 1.0, 0.6735, 0.7031, 0.7615, 0.8055, 0.8437, 0.8935, 0.9375, 0.9815, 1.0255, 1.0695, 1.1250, 1.1575, 1.2015,-1};
const double picture_rate_table[16] = {-1, 23.976, 24, 25, 29.97, 30, 50, 59.94, 60, -1, -1, -1, -1, -1, -1, -1};
uint8 default_intra_quantizer[8][8] = {{8, 16, 19, 22, 26, 27, 29, 34}, {16, 16, 22, 24, 27, 29, 34, 37}, {19, 22, 26, 27, 29, 34, 34, 38}, {22, 22, 26, 27, 29, 34, 37, 40}, {22, 26, 27, 29, 32, 35, 40 ,48}, {26, 27, 29, 32, 35, 40, 48, 58}, {26, 27, 29, 34, 38, 46, 56, 69}, {27, 29, 35, 38, 46, 56, 69, 83}};
uint8 default_non_intra_quantizer[8][8]={16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
pair<const char*,int> macroblockAddressIncrementTable[]={make_pair("1", 1),make_pair("011", 2),make_pair("010", 3),make_pair("0011", 4),make_pair("0010", 5),make_pair("00011", 6),make_pair("00010", 7),make_pair("0000111", 8),make_pair("0000110", 9),make_pair("00001011", 10),make_pair("00001010", 11),make_pair("00001001", 12),make_pair("00001000", 13),make_pair("00000111", 14),make_pair("00000110", 15),make_pair("0000010111", 16),make_pair("0000010110", 17),make_pair("0000010101", 18),make_pair("0000010100", 19),make_pair("0000010011", 20),make_pair("0000010010", 21),make_pair("00000100011", 22),make_pair("00000100010", 23),make_pair("00000100001", 24),make_pair("00000100000", 25),make_pair("00000011111", 26),make_pair("00000011110", 27),make_pair("00000011101", 28),make_pair("00000011100", 29),make_pair("00000011011", 30),make_pair("00000011010", 31),make_pair("00000011001", 32),make_pair("00000011000", 33),make_pair("00000001111", 34),make_pair("00000001000", 35)};
const int macroblocktypeTableSize[4] = {2,7,11,1};
pair<const char*,int> macroblocktypeTable[4][20]={{make_pair("1", 1),make_pair("01", 17)},
{make_pair("1", 10),make_pair("01", 2),make_pair("001", 8),make_pair("00011", 1),make_pair("00010", 26),make_pair("00001", 18),make_pair("000001", 17)},
{make_pair("10", 12),make_pair("11", 14),make_pair("010", 4),make_pair("011", 6),make_pair("0010", 8),make_pair("0011", 10),make_pair("00011", 1),make_pair("00010", 30),make_pair("000011", 26),make_pair("000010", 22),make_pair("000001", 17)},
{make_pair("1", 1)}};
// the table that used to decode motion horizontal forward backward code, motion vertical backward forward code
pair<const char*,int> motionVectorTable[]={make_pair("00000011001", -16),make_pair("00000011011", -15),make_pair("00000011101", -14),make_pair("00000011111", -13),make_pair("00000100001", -12),make_pair("00000100011", -11),make_pair("0000010011", -10),make_pair("0000010101", -9),make_pair("0000010111", -8),make_pair("00000111", -7),make_pair("00001001", -6),make_pair("00001011", -5),make_pair("0000111", -4),make_pair("00011", -3),make_pair("0011", -2),make_pair("011", -1),make_pair("1", 0),make_pair("010", 1),make_pair("0010", 2),make_pair("00010", 3),make_pair("0000110", 4),make_pair("00001010", 5),make_pair("00001000", 6),make_pair("00000110", 7),make_pair("0000010110", 8),make_pair("0000010100", 9),make_pair("0000010010", 10),make_pair("00000100010", 11),make_pair("00000100000", 12),make_pair("00000011110", 13),make_pair("00000011100", 14),make_pair("00000011010", 15),make_pair("00000011000", 16)};
pair<const char*, int> macroblockPatternTable[] = {make_pair("111", 60),make_pair("1101", 4),make_pair("1100", 8),make_pair("1011", 16),make_pair("1010", 32),make_pair("10011", 12),make_pair("10010", 48),make_pair("10001", 20),make_pair("10000", 40),make_pair("01111", 28),make_pair("01110", 44),make_pair("01101", 52),make_pair("01100", 56),make_pair("01011", 1),make_pair("01010", 61),make_pair("01001", 2),make_pair("01000", 62),make_pair("001111", 24),make_pair("001110", 36),make_pair("001101", 3),make_pair("001100", 63),make_pair("0010111", 5),make_pair("0010110", 9),make_pair("0010101", 17),make_pair("0010100", 33),make_pair("0010011", 6),make_pair("0010010", 10),make_pair("0010001", 18),make_pair("0010000", 34),make_pair("00011111", 7),make_pair("00011110", 11),make_pair("00011101", 19),make_pair("00011100", 35),make_pair("00011011", 13),make_pair("00011010", 49),make_pair("00011001", 21),make_pair("00011000", 41),make_pair("00010111", 14),make_pair("00010110", 50),make_pair("00010101", 22),make_pair("00010100", 42),make_pair("00010011", 15),make_pair("00010010", 51),make_pair("00010001", 23),make_pair("00010000", 43),make_pair("00001111", 25),make_pair("00001110", 37),make_pair("00001101", 26),make_pair("00001100", 38),make_pair("00001011", 29),make_pair("00001010", 45),make_pair("00001001", 53),make_pair("00001000", 57),make_pair("00000111", 30),make_pair("00000110", 46),make_pair("00000101", 54),make_pair("00000100", 58),make_pair("000000111", 31),make_pair("000000110", 47),make_pair("000000101", 55),make_pair("000000100", 59),make_pair("000000011", 27),make_pair("000000010", 39)};
pair<const char*, int> block_size_luminance[] = {make_pair("100", 0),make_pair("00", 1),make_pair("01", 2),make_pair("101", 3),make_pair("110", 4),make_pair("1110", 5),make_pair("11110", 6),make_pair("111110", 7),make_pair("1111110", 8)};
pair<const char*, int> block_size_chrominance[] = {make_pair("00", 0),make_pair("01", 1),make_pair("10", 2),make_pair("110", 3),make_pair("1110", 4),make_pair("11110", 5),make_pair("111110", 6),make_pair("1111110", 7),make_pair("11111110", 8)};
pair<const char*, int> dc_coef_table[]={make_pair("10", -1),make_pair("1", 1),make_pair("11", 1),make_pair("011", 65),make_pair("0100", 2),make_pair("0101", 129),make_pair("00101", 3),make_pair("00111", 193),make_pair("00110", 257),make_pair("000110", 66),make_pair("000111", 321),make_pair("000101", 385),make_pair("000100", 449),make_pair("0000110", 4),make_pair("0000100", 130),make_pair("0000111", 513),make_pair("0000101", 577),make_pair("000001", -2),make_pair("00100110", 5),make_pair("00100001", 6),make_pair("00100101", 67),make_pair("00100100", 194),make_pair("00100111", 641),make_pair("00100011", 705),make_pair("00100010", 769),make_pair("00100000", 833),make_pair("0000001010", 7),make_pair("0000001100", 68),make_pair("0000001011", 131),make_pair("0000001111", 258),make_pair("0000001001", 322),make_pair("0000001110", 897),make_pair("0000001101", 961),make_pair("0000001000", 1025),make_pair("000000011101", 8),make_pair("000000011000", 9),make_pair("000000010011", 10),make_pair("000000010000", 11),make_pair("000000011011", 69),make_pair("000000010100", 132),make_pair("000000011100", 195),make_pair("000000010010", 259),make_pair("000000011110", 386),make_pair("000000010101", 450),make_pair("000000010001", 514),make_pair("000000011111", 1089),make_pair("000000011010", 1153),make_pair("000000011001", 1217),make_pair("000000010111", 1281),make_pair("000000010110", 1345),make_pair("0000000011010", 12),make_pair("0000000011001", 13),make_pair("0000000011000", 14),make_pair("0000000010111", 15),make_pair("0000000010110", 70),make_pair("0000000010101", 71),make_pair("0000000010100", 133),make_pair("0000000010011", 196),make_pair("0000000010010", 323),make_pair("0000000010001", 578),make_pair("0000000010000", 642),make_pair("0000000011111", 1409),make_pair("0000000011110", 1473),make_pair("0000000011101", 1537),make_pair("0000000011100", 1601),make_pair("0000000011011", 1665),make_pair("00000000011111", 16),make_pair("00000000011110", 17),make_pair("00000000011101", 18),make_pair("00000000011100", 19),make_pair("00000000011011", 20),make_pair("00000000011010", 21),make_pair("00000000011001", 22),make_pair("00000000011000", 23),make_pair("00000000010111", 24),make_pair("00000000010110", 25),make_pair("00000000010101", 26),make_pair("00000000010100", 27),make_pair("00000000010011", 28),make_pair("00000000010010", 29),make_pair("00000000010001", 30),make_pair("00000000010000", 31),make_pair("000000000011000", 32),make_pair("000000000010111", 33),make_pair("000000000010110", 34),make_pair("000000000010101", 35),make_pair("000000000010100", 36),make_pair("000000000010011", 37),make_pair("000000000010010", 38),make_pair("000000000010001", 39),make_pair("000000000010000", 40),make_pair("000000000011111", 72),make_pair("000000000011110", 73),make_pair("000000000011101", 74),make_pair("000000000011100", 75),make_pair("000000000011011", 76),make_pair("000000000011010", 77),make_pair("000000000011001", 78),make_pair("0000000000010011", 79),make_pair("0000000000010010", 80),make_pair("0000000000010001", 81),make_pair("0000000000010000", 82),make_pair("0000000000010100", 387),make_pair("0000000000011010", 706),make_pair("0000000000011001", 770),make_pair("0000000000011000", 834),make_pair("0000000000010111", 898),make_pair("0000000000010110", 962),make_pair("0000000000010101", 1026),make_pair("0000000000011111", 1729),make_pair("0000000000011110", 1793),make_pair("0000000000011101", 1857),make_pair("0000000000011100", 1921),make_pair("0000000000011011", 1985)};
//picture coding type 0  us forbidden 1 is I type picture, 2 is P type picture, 3 is B type picture, the rest is preserved up to 15
#endif
//sequence_header_code