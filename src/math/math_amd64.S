.text
	.globl sqrt, sqrtf

	.type sqrt,@function
	sqrt:	
		sqrtsd %xmm0, %xmm0
		ret

	.type sqrtf,@function
	sqrtf:
		sqrtss %xmm0, %xmm0
		ret
