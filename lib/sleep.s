	.global	_sleep
_sleep
	mov	r1, ##60000000
	qmul	r0, r1
	getqx	r0
	waitx	r0
	jmp	lr
