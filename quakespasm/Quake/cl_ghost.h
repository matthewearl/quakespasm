#ifndef __CL_GHOST_H
#define __CL_GHOST_H


typedef struct {
    float time;
    unsigned short frame;
    float origin[3];
    float angle[3];
} ghostrec_t;


void Ghost_Load (const char *map_name);
void Ghost_Update (void);
void Ghost_Draw (void);


#endif /* __CL_GHOST_H */
