//Copyright HeathenUK 2023, others' copyrights (Envenomator, Dean Belfield, etc.) unaffected.

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <eZ80.h>
#include <defines.h>
#include "mos-interface.h"
#include "vdp.h"

// Parameters:
// - argc: Argument count
// - argv: Pointer to the argument string - zero terminated, parameters separated by spaces
//

void putch24(uint24_t w)
{
	uint24_t temp = w;
	
	putch(temp & 0xFF); // write LSB
	temp = temp >> 8;
	putch(temp & 0xFF);
	temp = temp >> 8;
	putch(temp & 0xFF);
	
	return;	
}

void putch16(uint16_t w) {

	putch(w & 0xFF); // write LSB
	putch(w >> 8);	 // write MSB	

}

uint8_t strtou8(const char *str) {
    uint8_t result = 0;
    const uint8_t maxDiv10 = 255 / 10;
    const uint8_t maxMod10 = 255 % 10;

    // Skip leading white spaces
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }

    // Convert digits
    while (*str >= '0' && *str <= '9') {
        uint8_t digit = *str - '0';
        if (result > maxDiv10 || (result == maxDiv10 && digit > maxMod10)) {
            return 255;
        }
        result = result * 10 + digit;
        str++;
    }

    return result;
}

uint24_t strtou24(const char *str) {
    uint32_t result = 0;
    const uint32_t maxDiv10 = 1677721;
    const uint32_t maxMod10 = 5;

    // Skip leading white spaces
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }

    // Convert digits
    while (*str >= '0' && *str <= '9') {
        uint32_t digit = *str - '0';
        if (result > maxDiv10 || (result == maxDiv10 && digit > maxMod10)) {
            return 16777215;  // Maximum 24-bit unsigned int
        }
        result = result * 10 + digit;
        str++;
    }

    return result;
}

#define CHUNK_SIZE 4096

void load_wav(const char * filename, int8_t sample_id) {

    uint16_t i;
    uint32_t data_size;
	uint24_t chunk, remaining_data, bytes_read = 0;
    unsigned char header[300];
    int8_t * sample_buffer;
	//int8_t sample_buffer[CHUNK_SIZE];
    uint8_t fp;
    FIL * fo;

    fp = mos_fopen(filename, 0x01);
    if (!fp) {
        printf("Error: could not open file %s\n", filename);
        return;
    }

    for (i = 0; i < 300 && !mos_feof(fp); i++) {
        header[i] = mos_fgetc(fp);
        if (i >= 7 && header[i - 7] == 'd' && header[i - 6] == 'a' && header[i - 5] == 't' && header[i - 4] == 'a') {
            break; // found the start of the data
        }
    }

    if (i >= 300 || !(header[0] == 'R' && header[1] == 'I' && header[2] == 'F' && header[3] == 'F') ||
        !(header[8] == 'W' && header[9] == 'A' && header[10] == 'V' && header[11] == 'E' &&
            header[12] == 'f' && header[13] == 'm' && header[14] == 't' && header[15] == ' ') ||
        !(header[16] == 0x10 && header[17] == 0x00 && header[18] == 0x00 && header[19] == 0x00 &&
            header[20] == 0x01 && header[21] == 0x00 && header[22] == 0x01 && header[23] == 0x00)) {
        printf("Error: invalid WAV file\n");
        mos_fclose(fp);
        return;
    }

    //sample_rate = header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24);
    data_size = header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24);

    if (!(header[34] == 8 && header[35] == 0)) {
        printf("Error: unsupported PCM format\n");
        mos_fclose(fp);
        return;
    }

	//printf("Uploading a sample with length %u\r\n", (header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24)));

	//printf("Made up of bytes %u and %u", ((header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24)) & 0xFFFF), ((header[i - 3] | (header[i - 2] << 8) | (header[i - 1] << 16) | (header[i] << 24)) >> 16));

    putch(23);
    putch(0);
    putch(133);
	putch(sample_id * -1); //Sample ID
    putch(5); //Sample mode
    putch(0); //Record mode

	putch16(data_size & 0xFFFF);
	putch(data_size >> 16);
		
	sample_buffer = (int8_t *) malloc(CHUNK_SIZE);
		
	remaining_data = data_size;
		
	while (remaining_data > 0) {
		
		if (remaining_data > CHUNK_SIZE) {
			chunk = CHUNK_SIZE;
		} else chunk = remaining_data;
		
		mos_fread(fp, sample_buffer, chunk);

			for (i = 0; i < chunk; ++i) {
				sample_buffer[i] -= 128;
			}

		mos_puts(sample_buffer, chunk, 0);
		remaining_data -= chunk;
	
	}
	//	printf("Uploaded a sample with length %u\r\n", data_size);
	free(sample_buffer);
    mos_fclose(fp);
}

void assign_sample(int8_t sample_id, uint8_t channel) {
	
	putch(23);
	putch(0);
	putch(133);
	putch(channel);
	putch(4);
	putch(sample_id * -1);
	
}

void play_sample(uint8_t channel, uint8_t volume, uint24_t duration) {
	
	putch(23);
	putch(0);
	putch(133);
	putch(channel);
	putch(0);
	putch(volume);
	putch16(0);
	putch16(duration);
	
}

int main(int argc, char * argv[])
{

	uint8_t sample_id, channel, volume;
	uint24_t duration;
	
	//printf("%u args\r\n", argc);
	//Args: 1=filenames, 2=sample slot, 3=channel, 4=volume
	
    if ((argc != 4) && (argc != 6)) {
        printf("Usage is %s <filename> <sample slot> <channel> [volume] [duration]\r\n", argv[0]);
        return 0;
    }
	
	sample_id = strtou8(argv[2]);
	channel = strtou8(argv[3]);
	
	if (argc == 4) {
		//printf("args: 1=%s, 2=%u, 3=%u", argv[1], sample_id, channel);
		load_wav(argv[1], sample_id);
		assign_sample(sample_id, channel);
	} else if (argc == 6) {
		volume = strtou8(argv[4]);
		duration = strtou24(argv[5]);
		//printf("args: 1=%s, 2=%u, 3=%u, 4=%u", argv[1], sample_id, channel, volume);
		load_wav(argv[1], sample_id);
		assign_sample(sample_id, channel);
		play_sample(sample_id, volume, duration);
	}
	
	return 0;
}