import java.io.InputStream;
import java.io.OutputStream;
import java.lang.System;

class Bootloader {
    public static void main(final String[] args) {
        System.out.println("hello world");

    }
}

final class Command {

    final int CMD_WRITE = 0x50;
    final int CMD_READ = 0x51;
    final int CMD_ERASE = 0x52;
    final int CMD_RESET = 0x53;
    final int CMD_JUMP = 0x54;
    final int CMD_VERIFY = 0x55;

    final int CMD_ACK = 0x90;
    final int CMD_NACK = 0x91;
    final int CMD_ERROR = 0x92;

    final int CMD_HELP = 0x40;

    final int SYNC_CHAR = '$';

}

class Flasher {

    private InputStream in_bytes;
    private OutputStream out_bytes;



}