// (C) 2016 Joshua Allen, MIT/x11 License.
//
//All MIT/x11 Licensed Code in this file may be relicensed freely under the GPL or LGPL licenses.

/* ootx data decoder */

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include "ootx_decoder.h"
#include "crc32.h"

//char* fmt_str = "L Y HMD %d 5 1 206230 %d\n";

#define MAX_BUFF_SIZE 1024

void (*ootx_packet_clbk)(ootx_packet* packet) = NULL;

void ootx_init_decoder_context(ootx_decoder_context *ctx) {
	ctx->buf_offset = 0;
	ctx->bits_written = 0;

	ctx->preamble = 0x00;
	ctx->bits_processed = 0;
	ctx->found_preamble = 0;

	ctx->buffer = (uint8_t*)malloc(MAX_BUFF_SIZE);
	ctx->payload_size = (uint16_t*)ctx->buffer;
	*(ctx->payload_size) = 0;
}
/*
void ootx_init_buffer() {
	buffer = (uint8_t*)malloc(MAX_BUFF_SIZE);
	payload_size = (uint16_t*)buffer;
	*payload_size = 0;
}
*/
uint8_t ootx_decode_bit(uint32_t length) {
	length = ((length/500)*500)+500;

	length-=3000;
	if (length>=2000) { length-=2000; }
	if (length>=1000)  { return 0xFF; }

	return 0x00;
}

uint8_t ootx_detect_preamble(ootx_decoder_context *ctx, uint8_t dbit) {
	ctx->preamble <<= 1;
	ctx->preamble |= (0x01 & dbit);
	if ((ctx->preamble & 0x0001ffff) == 0x01) return 1;
	return 0;
}

void ootx_reset_buffer(ootx_decoder_context *ctx) {
	ctx->buf_offset = 0;
	ctx->buffer[ctx->buf_offset] = 0;
	ctx->bits_written = 0;
	*(ctx->payload_size) = 0;
}

void ootx_inc_buffer_offset(ootx_decoder_context *ctx) {
	++(ctx->buf_offset);
//	if (buf_offset>=MAX_BUFF_SIZE) buf_offset = 0;
	if(ctx->buf_offset>=MAX_BUFF_SIZE) {
		ctx->found_preamble = 0;
	}
	assert(ctx->buf_offset<MAX_BUFF_SIZE);
	ctx->buffer[ctx->buf_offset] = 0;
}

void ootx_write_to_buffer(ootx_decoder_context *ctx, uint8_t dbit) {
	uint8_t *current_byte = ctx->buffer + ctx->buf_offset;
//	printf("%d\n", dbit);
	*current_byte >>= 1;
	*current_byte |= (0x80 & dbit);
	++(ctx->bits_written);
	if (ctx->bits_written>7) {
		ctx->bits_written=0;
//		printf("%d\n", *current_byte);
		ootx_inc_buffer_offset(ctx);
	}
}

void ootx_process_bit(ootx_decoder_context *ctx, uint32_t length) {
	uint8_t dbit = ootx_decode_bit(length);
	++(ctx->bits_processed);

//	printf("z %d %d\n", bits_processed,dbit);
//	printf("d %d\n", bits_processed,dbit);

	if ( ootx_detect_preamble(ctx, dbit) ) {
		/*	data stream can start over at any time so we must
			always look for preamble bits */
//		printf("Preamble found\n");
		ootx_reset_buffer(ctx);
		ctx->bits_processed = 0;
		ctx->found_preamble = 1;
	}
	else if(ctx->bits_processed>16) {
		//every 17th bit needs to be dropped
//		printf("drop %d\n", dbit);
		ctx->bits_processed = 0;
	}
	else
	{
		ootx_write_to_buffer(ctx, dbit);

		if (ctx->buf_offset >= (*(ctx->payload_size)+6)) {
			/*	once we have a complete ootx packet, send it out in the callback */
			ootx_packet op;

			op.length = *(ctx->payload_size);
			op.data = ctx->buffer+2;
			op.crc32 = *(uint32_t*)(ctx->buffer+2+op.length);

			uint32_t crc = crc32(0xffffffff,op.data,op.length);

			if (crc != op.crc32) {
				printf("CRC mismatch\n");
			}

			if ((crc == op.crc32) && ootx_packet_clbk) ootx_packet_clbk(&op);

			ootx_reset_buffer(ctx);
		}
	}
}
