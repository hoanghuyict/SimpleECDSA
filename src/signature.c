#include <stdlib.h>
#include <stdio.h>
#include <gmp.h>
#include <stdbool.h>
#include <assert.h>
#include "domain_parameters.h"
#include "point.h"
#include "signature.h"
#include "numbertheory.h"
#include "random.h"

/*Initialize a signature*/
signature signature_init()
{
	signature sig;
	sig = malloc(sizeof(struct signature_s));
	mpz_init(sig->r);
	mpz_init(sig->s);
	return sig;
}

/*Print signature to standart output stream*/
void signature_print(signature sig)
{
	printf("\nSignature (r,s): \n\t(");
	mpz_out_str(stdout, 10, sig->r);
	printf(",\n\t");
	mpz_out_str(stdout, 10, sig->s);
	printf(")\n");
}

/*Set signature from strings of a base from 2-62*/
void signature_set_str(signature sig, char *r, char *s, int base)
{
	mpz_set_str(sig->r, r, base);
	mpz_set_str(sig->s, s, base);
}

/*Set signature from hexadecimal strings*/
void signature_set_hex(signature sig, char *r, char *s)
{
	signature_set_str(sig,r,s,16);
}

/*Set signature from decimal unsigned long ints*/
void signature_set_ui(signature sig, unsigned long int r, unsigned long int s)
{
	mpz_set_ui(sig->r, r);
	mpz_set_ui(sig->s, s);
}

/*Make R a copy of P*/
void signature_copy(signature R, signature sig)
{
	mpz_set(R->r, sig->r);
	mpz_set(R->s, sig->s);
}


/*Compare two signatures return 1 if not the same, returns 0 if they are the same*/
bool signature_cmp(signature sig1, signature sig2)
{
	return !mpz_cmp(sig1->r,sig2->r) && !mpz_cmp(sig1->s,sig2->s);
}

/*Generates a public key for a private key*/
void signature_generate_key(point public_key, mpz_t private_key, domain_parameters curve)
{
	point_multiplication(public_key, private_key, curve->G, curve);
}

/*Generate signature for a message*/
void signature_sign(signature sig, mpz_t message, mpz_t private_key, domain_parameters curve)
{
	//message must not have a bit length longer than that of n
	//see: Guide to Elliptic Curve Cryptography, section 4.4.1.
	assert(mpz_sizeinbase(message, 2) <= mpz_sizeinbase(curve->n, 2));
	
	//Initializing variables
	mpz_t k;mpz_init(k);
	mpz_t x;mpz_init(x);
	point Q = point_init();
	mpz_t r;mpz_init(r);
	mpz_t t1;mpz_init(t1);
	mpz_t t2;mpz_init(t2);
	mpz_t t3;mpz_init(t3);
	mpz_t s;mpz_init(s);

	gmp_randstate_t r_state;

	signature_sign_start:

	//Set k
	gmp_randinit_default(r_state);
	random_seeding(r_state);
	mpz_sub_ui(t1, curve->n, 2);
	mpz_urandomm(k , r_state , t1);
	gmp_randclear(r_state);

	//Calculate x
	point_multiplication(Q, k, curve->G, curve);
	mpz_set(x, Q->x);
	point_clear(Q);

	//Calculate r
	mpz_mod(r, x, curve->n);
	if(!mpz_sgn(r))	//Start over if r=0, note haven't been tested memory might die :)
		goto signature_sign_start;
	mpz_clear(x);

	//Calculate s
	//s = k¯¹(e+d*r) mod n = (k¯¹ mod n) * ((e+d*r) mod n) mod n
	number_theory_inverse(t1, k, curve->n);//t1 = k¯¹ mod n
	mpz_mul(t2, private_key, r);//t2 = d*r
	mpz_add(t3, message, t2);	//t3 = e+t2
	mpz_mod(t2, t3, curve->n);	//t2 = t3 mod n
	mpz_mul(t3, t2, t1);		//t3 = t2 * t1
	mpz_mod(s, t3, curve->n);	//s = t3 mod n
	mpz_clear(t1);
	mpz_clear(t2);
	mpz_clear(t3);

	//Set signature
	mpz_set(sig->r, r);
	mpz_set(sig->s, s);

	//Release k,r and s
	mpz_clear(k);
	mpz_clear(r);
	mpz_clear(s);
}

/*Verify the integrity of a message using it's signature*/
bool signature_verify(mpz_t message, signature sig, point public_key, domain_parameters curve)
{
	//verify r and s are within [1, n-1]
	mpz_t one;mpz_init(one);
	mpz_set_ui(one, 1);
	if(	mpz_cmp(sig->r,one) < 0 &&
		mpz_cmp(curve->n,sig->r) <= 0 &&
		mpz_cmp(sig->s,one) < 0 &&
		mpz_cmp(curve->n,sig->s) <= 0)
	{
		mpz_clear(one);
		return false;
	}

	mpz_clear(one);
	
	//Initialize variables
	mpz_t w;mpz_init(w);
	mpz_t u1;mpz_init(u1);
	mpz_t u2;mpz_init(u2);
	mpz_t t;mpz_init(t);
	mpz_t tt2;mpz_init(tt2);
	point x = point_init();
	point t1 = point_init();
	point t2 = point_init();

	//w = s¯¹ mod n
	number_theory_inverse(w, sig->s, curve->n);
	
	//u1 = message * w mod n
	mpz_mod(tt2, message, curve->n);
	mpz_mul(t, tt2, w);
	mpz_mod(u1, t, curve->n);

	//u2 = r*w mod n
	mpz_mul(t, sig->r, w);
	mpz_mod(u2, t, curve->n);

	//x = u1*G+u2*Q
	point_multiplication(t1, u1, curve->G, curve);
	point_multiplication(t2, u2, public_key, curve);
	point_addition(x, t1, t2, curve);

	//Get the result, by comparing x value with r and verifying that x is NOT at infinity
	bool result = mpz_cmp(sig->r, x->x) == 0 && !x->infinity;

	//release memory
	point_clear(x);
	point_clear(t1);
	point_clear(t2);
	mpz_clear(w);
	mpz_clear(u1);
	mpz_clear(u2);
	mpz_clear(t);
	mpz_clear(tt2);

	//Return result
	return result;
}

/*Release signature*/
void signature_clear(signature sig)
{
	mpz_clear(sig->r);
	mpz_clear(sig->s);
	free(sig);
}
