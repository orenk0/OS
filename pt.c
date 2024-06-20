#include "os.h"

void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {

    //chop into 5 pieces of size 9:
    uint64_t chunks[5];
    for(int i = 4; i >= 0; i--){//store from left to right
        chunks[i] = vpn & 0x1FF;
        vpn >>= 9;
    }
    int reached_level_4 = 1;
    //access the root:
    void* root = phys_to_virt(pt<<12);//add offset 0 for first in this page.
    uint64_t* cur = (uint64_t*)root;//the current page in our walk.
    for(int i = 0; i <= 3 ; i++){//last level is different.
        cur += (chunks[i]);//desired entry (64 bit = 8 bytes).
        uint64_t tmp = *cur;//value of the pte
        if(tmp&1) cur = (uint64_t*)(phys_to_virt(tmp^1));//advance. offset 0 for first in page.
        else{//not valid
            if(ppn!=NO_MAPPING){//need to create new pt node.
                *cur = ((alloc_page_frame()<<12)|1); //valid.
                cur = (uint64_t*)(phys_to_virt((*cur)^1));
            }else{
                reached_level_4 = 0;
                break;// there's no mapping to delete.
            }
        }
    }
    if(reached_level_4){//last level in the trie. almost the same logic:
        cur += (chunks[4]);
        *cur = (ppn<<12);//not valid yet.
        if(ppn != NO_MAPPING){
            *cur |= 1;//now valid.
        }

    }



}

uint64_t page_table_query(uint64_t pt, uint64_t vpn) {

    //chop into 5 pieces of size 9:
    uint64_t chunks[5];
    for(int i = 4; i >= 0; i--){//store from left to right
        chunks[i] = vpn & 0x1FF;
        vpn >>= 9;
    }
    //access the root:
    void* root = phys_to_virt(pt<<12);//add offset 0 for first in this page.
    uint64_t* cur = (uint64_t*)root;//the current page in our walk.
    for(int i = 0; i <= 3 ; i++){//last level is different.
        cur += (chunks[i]);//desired entry (64 bit = 8 bytes).
        uint64_t tmp = *cur;//value of the pte
        if(tmp&1) cur = (uint64_t*)(phys_to_virt(tmp^1));//advance. offset 0 for first in page.
        else return NO_MAPPING;
    }
    
    cur += (chunks[4]);
    uint64_t tmp = *cur;
    if(tmp&1) return tmp>>12;
    else return NO_MAPPING;

}
