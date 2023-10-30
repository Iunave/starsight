%ifndef __FUNCTIONS__
%define __FUNCTIONS__

section .text

    global hash_crc32 ;const void*, uint64 bytes -> uint64
hash_crc32:
    xor rax, rax
.hash_qword:
    test rsi, -8
    jz .hash_byte
    crc32 rax, qword[rdi]
    add rdi, 8
    sub rsi, 8
    jmp .hash_qword
.hash_byte:
    test rsi, rsi
    jz .done
    crc32 rax, byte[rdi]
    inc rdi
    dec rsi
    jmp .hash_byte
.done:
    ret

    global hash_crc64 ;const void*, uint64 bytes -> uint64
hash_crc64:
    mov rdx, rsi
    xor r8, r8
    shr rdx, 1
    setc r8b ; account for uneven size
    cmp rdx, 8 ; single call is enough
    jbe hash_crc32
    mov rsi, rdx
    call hash_crc32
    mov ecx, eax
    mov rsi, rdx
    add rsi, r8
    call hash_crc32
    shl rax, 32
    or rax, rcx
    ret

    global count_mipmaps ;uint32 width, uint32 height -> uint32
count_mipmaps:
    cmp edi, esi
    cmovb edi, esi
    xor esi, esi
    test edi, edi
    setz sil
    add edi, esi ;0 -> 1
    bsr eax, edi
    inc eax ;account for original level
    ret

	global abs_flt64
abs_flt64:
	vpcmpeqq ymm1, ymm1
	vpsrlq ymm1, 1
	vpand ymm0, ymm1
	ret

	global abs_flt32
abs_flt32:
	vpcmpeqq ymm1, ymm1
	vpsrld ymm1, 1
	vpand ymm0, ymm1
	ret

	global abs_int64
abs_int64:
	mov rax, rdi
	neg rdi
	test rax, rax
	cmovs rax, rdi
	ret

	global rand_normal; -> float64 (0 to 1)
rand_normal:
	rdrand rax
	jnc rand_normal
	mov rdi, 0x7FFFFFFFFFFFFFFF
	and rax, rdi
	vcvtsi2sd xmm0, rax
	vcvtsi2sd xmm1, rdi
	vdivsd xmm0, xmm1
	ret

	global randrange_flt ; float64, float64 -> float64
randrange_flt:
	vmovsd xmm2, xmm0
	vmovsd xmm3, xmm1
	call rand_normal
	vsubsd xmm3, xmm2
	vfmadd132sd xmm0, xmm2, xmm3
	ret

	global randrange_int ; int64, int64 -> int64
randrange_int:
	vcvtsi2sd xmm2, rdi
	vcvtsi2sd xmm3, rsi
	call rand_normal
	vsubsd xmm3, xmm2
	vfmadd132sd xmm0, xmm2, xmm3
	vcvtsd2si rax, xmm0
	ret

%endif


