#ifndef MATH_H
#define MATH_H

static int mathMax(int v1,int v2){
    return v1 > v2 ? v1 : v2;
}

static unsigned mathHash(unsigned x){
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
	return x;
}

static unsigned mathRnd(void){
    static unsigned v = 2463534242u;
    v = mathHash(v);
    return v;
}

#endif
