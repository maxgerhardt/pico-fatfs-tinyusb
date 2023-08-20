#include <Arduino.h>
#include <FatFs.h>
#include <Adafruit_TinyUSB.h>

FATFS fatfs;
uint8_t fatfs_ram_buf[128 * 1024];
static bool ram_initted = false;
static bool fs_is_ready = false;
#include "ff15/ff.h"			/* Obtains integer types */
#include "ff15/diskio.h"		/* Declarations of disk functions */
extern "C" DSTATUS RAM_disk_status() {
    Serial.println("RAM_disk_status");
    return RES_OK;
}
extern "C" DSTATUS RAM_disk_initialize() {
    Serial.println("RAM_disk_initialize");
    if(!ram_initted) {
        memset(fatfs_ram_buf, 0, sizeof(fatfs_ram_buf));
        ram_initted = true;
    }
    return RES_OK;
}
extern "C"  DRESULT RAM_disk_read(
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
) {
    Serial.println("RAM_disk_read():  sector " + String(sector) + " count " + String(count));
    uint8_t* source_addr = &fatfs_ram_buf[sector * FF_MAX_SS];
    size_t read_amount = count * FF_MAX_SS;
    if((source_addr + read_amount) > (fatfs_ram_buf + sizeof(fatfs_ram_buf))) {
        return RES_ERROR; 
    }
    memcpy(buff, source_addr, read_amount);
    return RES_OK;
}
extern "C" DRESULT RAM_disk_write(
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
) {
    Serial.println("RAM_disk_write():  sector " + String(sector) + " count " + String(count));
    uint8_t* target_addr = &fatfs_ram_buf[sector * FF_MAX_SS];
    size_t write_amount = count * FF_MAX_SS;
    if((target_addr + write_amount) > (fatfs_ram_buf + sizeof(fatfs_ram_buf))) {
        return RES_ERROR; 
    }
    memcpy(target_addr, buff, write_amount);
    return RES_OK;
}
extern "C" DRESULT RAM_disk_ioctl(BYTE cmd, void* buff) {
    switch(cmd) {
        case CTRL_SYNC: 
            return RES_OK; /* nothing to do */
        case GET_SECTOR_COUNT:
            *(DWORD*)buff = sizeof(fatfs_ram_buf) / FF_MAX_SS; 
            Serial.println("Returned sector count " + String(*(DWORD*)buff));
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1; 
            return RES_OK;
        default:
            return RES_PARERR;
    }
}

extern "C" DWORD get_fattime (void) {
    time_t t;
    struct tm *stm;


    t = time(0);
    stm = localtime(&t);

    return (DWORD)(stm->tm_year - 80) << 25 |
           (DWORD)(stm->tm_mon + 1) << 21 |
           (DWORD)stm->tm_mday << 16 |
           (DWORD)stm->tm_hour << 11 |
           (DWORD)stm->tm_min << 5 |
           (DWORD)stm->tm_sec >> 1;
}

const char* err_to_str (FRESULT rc)
{
	const char *str =
		"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
		"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
		"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
		"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0" "INVALID_PARAMETER\0";
	int i;
	for (i = 0; i != (int) rc && *str; i++) {
		while (*str++) ;
	}
    return str;
}

// Invoked when received Test Unit Ready command.
// return true allowing host to read/write this LUN e.g SD card inserted
extern "C" bool my_tud_msc_test_unit_ready_cb() {
    return fs_is_ready;
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and return number of copied bytes.
extern "C" int32_t my_tud_msc_read10_cb(uint32_t lba, void* buffer, uint32_t bufsize) {
    if (!fs_is_ready || (lba >= sizeof(fatfs_ram_buf) / FF_MAX_SS)) {
        return -1;
    }
    RAM_disk_read((BYTE*)buffer, lba, bufsize/FF_MAX_SS);
    return bufsize;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and return number of written bytes
extern "C" int32_t my_tud_msc_write10_cb(uint32_t lba, uint8_t* buffer, uint32_t bufsize) {
    if (!fs_is_ready || (lba >= sizeof(fatfs_ram_buf) / FF_MAX_SS)) {
        return -1;
    }
    RAM_disk_write(buffer, lba, bufsize/FF_MAX_SS);
    return bufsize;
}


Adafruit_USBD_MSC usb_msc;
void msc_flush_cb (void) { /* NOP */}
bool is_writable() { return true; }

void setup() {
    Serial.begin(115200);
    //while(!Serial);
    FRESULT res;
    const MKFS_PARM formatopt = {
        FM_ANY, 
        1, /* n_fat*/ 
        0, /* algin */ 
        0, /* n_root*/ 
        0 /*au_size */
    };
    static uint8_t work_buf[2*FF_MAX_SS];
    res = f_mkfs("0:", &formatopt, work_buf, sizeof(work_buf));
    Serial.println("Formatting filesystem: " + String(err_to_str(res)));

    res = f_mount(&fatfs, "", 1 /* force mount*/);    
    Serial.println("Mounting FatFS: " + String(err_to_str(res)));

    FIL fp {};
    res = f_open(&fp, "test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    Serial.println("Opening File: " + String(err_to_str(res)));
    const char* content = "Hello from Pico FatFS";
    UINT bytes_written;
    res = f_write(&fp, content, strlen(content), &bytes_written);
    Serial.println("Writing File: " + String(err_to_str(res)) + " num bytes written: "  + String(bytes_written));
    res = f_close(&fp);
    Serial.println("Closing file File: " + String(err_to_str(res)) + " num bytes written: "  + String(bytes_written));
    fs_is_ready = true;

    usb_msc.setID("Pico", "Internal Flash", "1.0");
    usb_msc.setCapacity(sizeof(fatfs_ram_buf)/FF_MAX_SS, FF_MAX_SS);
    usb_msc.setReadWriteCallback(my_tud_msc_read10_cb, my_tud_msc_write10_cb, msc_flush_cb);
    usb_msc.setWritableCallback(is_writable);
    usb_msc.setUnitReady(true);
    bool ok = usb_msc.begin();
    Serial.println("MSC begin ok: " + String(ok));
}

void loop() {
    Serial.println("Hello");
    delay(1000);
}