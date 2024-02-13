	.text
	.attribute	4, 16
	.attribute	5, "rv64i2p1_zba1p0_zbb1p0_zbc1p0"
	.file	"ctz_uncountable_wc.c"
	.globl	count_trailing_zeroes           # -- Begin function count_trailing_zeroes
	.p2align	1
	.type	count_trailing_zeroes,@function
count_trailing_zeroes:                  # @count_trailing_zeroes
	.cfi_startproc
# %bb.0:                                # %.loopexit
	snez	a1, a0
	andi	a2, a0, 1
	seqz	a2, a2
	and	a1, a1, a2
	ctzw	a0, a0
	neg	a1, a1
	and	a0, a0, a1
	ret
.Lfunc_end0:
	.size	count_trailing_zeroes, .Lfunc_end0-count_trailing_zeroes
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	1
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %count_trailing_zeroes.exit
	addi	sp, sp, -16
	.cfi_def_cfa_offset 16
	sd	ra, 8(sp)                       # 8-byte Folded Spill
	.cfi_offset ra, -8
	ld	a0, 8(a1)
	li	a2, 10
	li	a1, 0
	call	strtol@plt
	sext.w	a1, a0
	snez	a1, a1
	andi	a2, a0, 1
	seqz	a2, a2
	and	a1, a1, a2
	ctzw	a0, a0
	neg	a1, a1
	and	a0, a0, a1
	ld	ra, 8(sp)                       # 8-byte Folded Reload
	addi	sp, sp, 16
	ret
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc
                                        # -- End function
	.ident	"Ubuntu clang version 17.0.6 (++20231209124227+6009708b4367-1~exp1~20231209124336.77)"
	.section	".note.GNU-stack","",@progbits
