package com.example.stm32_flasher;

public class CRC8 {

    /*Maxim APPLICATION NOTE 27 */
    private static final byte[] CRC8_Table =
            {
                    0, 94, (byte) 188, (byte) 226, 97, 63, (byte) 221, (byte) 131, (byte) 194, (byte) 156, 126, 32, (byte) 163, (byte) 253, 31, 65,
                    (byte) 157, (byte) 195, 33, 127, (byte) 252, (byte) 162, 64, 30, 95, 1, (byte) 227, (byte) 189, 62, 96, (byte) 130, (byte) 220,
                    35, 125, (byte) 159, (byte) 193, 66, 28, (byte) 254, (byte) 160, (byte) 225, (byte) 191, 93, 3, (byte) 128, (byte) 222, 60, 98,
                    (byte) 190, (byte) 224, 2, 92, (byte) 223, (byte) 129, 99, 61, 124, 34, (byte) 192, (byte) 158, 29, 67, (byte) 161, (byte) 255,
                    70, 24, (byte) 250, (byte) 164, 39, 121, (byte) 155, (byte) 197, (byte) 132, (byte) 218, 56, 102, (byte) 229, (byte) 187, 89, 7,
                    (byte) 219, (byte) 133, 103, 57, (byte) 186, (byte) 228, 6, 88, 25, 71, (byte) 165, (byte) 251, 120, 38, (byte) 196, (byte) 154,
                    101, 59, (byte) 217, (byte) 135, 4, 90, (byte) 184, (byte) 230, (byte) 167, (byte) 249, 27, 69, (byte) 198, (byte) 152, 122, 36,
                    (byte) 248, (byte) 166, 68, 26, (byte) 153, (byte) 199, 37, 123, 58, 100, (byte) 134, (byte) 216, 91, 5, (byte) 231, (byte) 185,
                    (byte) 140, (byte) 210, 48, 110, (byte) 237, (byte) 179, 81, 15, 78, 16, (byte) 242, (byte) 172, 47, 113, (byte) 147, (byte) 205,
                    17, 79, (byte) 173, (byte) 243, 112, 46, (byte) 204, (byte) 146, (byte) 211, (byte) 141, 111, 49, (byte) 178, (byte) 236, 14, 80,
                    (byte) 175, (byte) 241, 19, 77, (byte) 206, (byte) 144, 114, 44, 109, 51, (byte) 209, (byte) 143, 12, 82, (byte) 176, (byte) 238,
                    50, 108, (byte) 142, (byte) 208, 83, 13, (byte) 239, (byte) 177, (byte) 240, (byte) 174, 76, 18, (byte) 145, (byte) 207, 45, 115,
                    (byte) 202, (byte) 148, 118, 40, (byte) 171, (byte) 245, 23, 73, 8, 86, (byte) 180, (byte) 234, 105, 55, (byte) 213, (byte) 139,
                    87, 9, (byte) 235, (byte) 181, 54, 104, (byte) 138, (byte) 212, (byte) 149, (byte) 203, 41, 119, (byte) 244, (byte) 170, 72, 22,
                    (byte) 233, (byte) 183, 85, 11, (byte) 136, (byte) 214, 52, 106, 43, 117, (byte) 151, (byte) 201, 74, 20, (byte) 246, (byte) 168,
                    116, 42, (byte) 200, (byte) 150, 21, 75, (byte) 169, (byte) 247, (byte) 182, (byte) 232, 10, 84, (byte) 215, (byte) 137, 107, 53};

    public static byte getCRC8(byte[] data, int len) {
        byte crc = 0;
        for (int i = 0; i < len; i++) {
            crc = (CRC8_Table[crc ^ data[i]]);
        }
        return crc;
    }

}
