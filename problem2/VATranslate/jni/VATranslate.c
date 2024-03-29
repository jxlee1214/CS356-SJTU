#include<stdio.h>
#include<stdlib.h>
#include<sys/mman.h>
#define pgd_index(va,loinfo) ((va)>>loinfo.pgdir_shift)
#define pte_index(va,loinfo) (((va)>>loinfo.page_shift)&((1<<(loinfo.pmd_shift-loinfo.page_shift))-1))
#define no_such_pid 1
#define no_mem_info 2
#define allo_fail   3
#define range_fault 4
#define walk_fail   5
#define cp_fail     -1

struct pagetable_layout_info{
    uint32_t pgdir_shift;
    uint32_t pmd_shift;
    uint32_t page_shift;
};

int main(int argc,char **argv)
{
    unsigned long va;
    unsigned long *table_addr;
    unsigned long *fake_pgd_addr;
    pid_t pid;
    struct pagetable_layout_info loinfo;
    int i;
    unsigned page_size;
    unsigned long pgd_ind,phy_addr;
    unsigned long *phy_base;
    unsigned long mask;
    int err=0;

    //check the arguments.
    if(argc!=3)
    {
        printf("Wrong arguments!\n");
        printf("Usage: ./VATranslate pid va\n");
        printf("Example: ./VATranslate 1 8001\n");
        return -1;
    }
    pid=atoi(argv[1]);
    va=strtoul(argv[2],NULL,16);

    //get the layout.
    err=syscall(304,&loinfo,4*3);

    if(err<0)
    {
        printf("copy to user failed!\n");
        return -1;
    }


    err=0;
    //printf("size:%d",sizeof(unsigned long));
    //print the layout.
    printf("pgdir_shift:%d, pmd_shift:%d, page_shift:%d\n",loinfo.pgdir_shift,loinfo.pmd_shift,loinfo.page_shift);

    //page size
    page_size=1<<(loinfo.page_shift);

    //mask is used to mask the lower bit of page table entry.
    mask=page_size-1;

    //allocate enough memory space.
    table_addr=mmap(NULL,page_size,PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    fake_pgd_addr=malloc(sizeof(unsigned long)*page_size);

    if(!table_addr||!fake_pgd_addr)
    {
        printf("allocate memory failed!\n");
        return -1;
    }

    err=syscall(305,pid,fake_pgd_addr,0,table_addr,va,va+1);
    //error handler.
    switch(err)
    {
        case no_such_pid:
        printf("Process with pid=%d doesn't exist.\n",pid);
        return -1;
        case no_mem_info:
        printf("Target process has no virtual memory infomation.\n");
        return -1;
        case allo_fail:
        printf("allocate memory failed.\n");
        return -1;
        case walk_fail:
        printf("walk_page_range failed.\n");
        return -1;
        case cp_fail:
        printf("copy to user failed!\n");
        return -1;
    }

    //get pgd index.
    pgd_ind=pgd_index(va,loinfo);
    //printf("pgd_index:%08lx\n",pgd_ind);

    //get physical base of page table.
    phy_base=fake_pgd_addr[pgd_ind];
    if(phy_base)
    {
        //get the entry in the table.
        phy_addr=phy_base[pte_index(va,loinfo)];
        //printf("pgtb[%d]=%08lx\n",pte_index(va,loinfo),phy_addr);

        //mask the lower bit of the entry.
        phy_addr=phy_addr&~mask;
        if(phy_addr)
        {
            //add the offset.
            phy_addr=va&mask|phy_addr;
            printf("virtural address:0x%08lx ===> physical address:0x%08lx\n",va,phy_addr);
        }
        else printf("virtual address:0x%08lx is not in the memory.\n",va);
    }
    else printf("virtual address:0x%08lx is not in the memory.\n",va);
    //free memory space.
    free(fake_pgd_addr);
    munmap(table_addr,page_size);
    return 0;
}
