#include <iostream>
#include "hashtable.h"
#include <stdlib.h>
#include <assert.h>

static void h_init(struct HTab* htab,size_t n){
    assert(n>0 && ((n-1)&n)==0);
    htab->tab = (HNode**) calloc(n,sizeof(HNode*));
    htab->size = 0;
    htab->mask = n-1;
}

static void h_insert(struct HTab* htab,struct HNode* node){
    size_t pos = (node->hcode)&htab->mask;
    HNode* next = htab->tab[pos];
    node->next = next;
    htab->tab[pos] = node;
    htab->size++;
}

int main(){

}