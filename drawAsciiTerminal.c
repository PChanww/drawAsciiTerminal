#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

//#define debug


#define SIZE 64
#define ttf_path "/opt/just/TIMESI.TTF"

struct TtfFile
{
   uint8_t* fileptr;
   long filesize;
   uint32_t ofs;
};


struct TableDirectory
{
    __uint32_t  sfntVersion;
    __uint16_t  numTables;
    __uint16_t  searchRange;
    __uint16_t  entryselector;
    __uint16_t  rangeShift;
    __uint32_t  rableRecode[0];
};

struct TableRecord
{
    uint8_t tag[4];
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
};

struct character_info
{
    struct TableRecord cmap;  
    struct TableRecord head;    
    struct TableRecord maxp;   
    struct TableRecord loca;    
    struct TableRecord glyf;    
    struct TableRecord hhea;   
    struct TableRecord hmtx;    
};

struct GlyphTable
{   
    int totalPoints;
    int16_t xyRANGE[4];
    uint16_t numberOfContours;          
    uint16_t *endPoints; 
    uint8_t  *flags;
    int16_t  *xCoords;
    int16_t  *yCoords;
};

struct GIDTable_info
{
   uint16_t* GIDTable;
   long segCount; 
};

struct glyf_info
{
   uint16_t glyf_ofs;
   int glyf_len; 
};



int16_t BE16(int16_t X){
    uint16_t v = (uint16_t)X;
    return (__int16_t)( (v >> 8) |
                        (v << 8));
}

int32_t BE32(int32_t X){
    uint32_t v = (uint32_t)X;
    return (int32_t)((v << 24) & 0xFF000000) |
                    ((v <<  8) & 0x00FF0000) |
                    ((v >>  8) & 0x0000FF00) |
                    ((v >> 24) & 0x000000FF);
}

int get_tff_file(char* TtfPath, struct TtfFile* ttffile){
    uint64_t filesize;
    int ret = 0;
    FILE* f = fopen( TtfPath, "rb");
    if (f == NULL){
        return 1;
    }
    if (fseek(f, 0, SEEK_END)){
        fclose(f);
        return 1;
    }
    ttffile->filesize = ftell(f);
    ttffile->fileptr = (uint8_t *)malloc(ttffile->filesize);
    if(ttffile->fileptr == NULL){
        fclose(f);
        return 1;
    }
    rewind(f);
    int read_len = fread(ttffile->fileptr, 1, ttffile->filesize, f);
    if (read_len != ttffile->filesize) {
        free(ttffile->fileptr);
        ttffile->fileptr = NULL;
        fclose(f);
        return 1; 
    }
    ret = fclose(f);

    return ret;
}

void get_GlyphTable(struct TtfFile* ttffile, struct character_info * info, struct GIDTable_info* GIDTable){
    
    
    uint16_t numTables;
    int32_t cmapTableOfs = 0;
    int32_t campTableLen = 0;

    ttffile->ofs = info->cmap.offset + 2;
    memcpy(&numTables,ttffile->fileptr + ttffile->ofs,2);
    numTables = BE16(numTables);

    uint8_t *encodingRecordSubTable = (uint8_t*)malloc(8 * numTables);

    ttffile->ofs += 2;
    memcpy(encodingRecordSubTable,ttffile->fileptr + ttffile->ofs,8 * numTables);
    for(int i = 0 ; i < numTables ; i++){

#ifdef debug
    printf("头表：\n");
    printf("%d\n", BE16(*(uint16_t*)(encodingRecordSubTable + 0 + i * 8)));
    printf("%d\n", BE16(*(uint16_t*)(encodingRecordSubTable + 2+ i * 8)));
    printf("0x%x\n", BE32(*(uint32_t*)(encodingRecordSubTable + 4+ i * 8)));
#endif

    if(BE16(*(uint16_t*)(encodingRecordSubTable + 0 + i * 8)) == 0 || BE16(*(uint16_t*)(encodingRecordSubTable + 2+ i * 8)) ==3){

        cmapTableOfs = BE32(*(uint32_t*)(encodingRecordSubTable + 4+ i * 8));
        // campTableLen = BE32(*(uint32_t*)(encodingRecordSubTable + 12+ i * 8)) - BE32(*(uint32_t*)(encodingRecordSubTable + 4+ i * 8));
        break;
        }
    }
    ttffile->ofs = info->cmap.offset + cmapTableOfs;
    if(BE16(*(uint16_t*)(ttffile->fileptr + ttffile->ofs)) == 4)
    {
        int segCount = BE16(*(uint16_t*)(ttffile->fileptr + ttffile->ofs + 6)) / 2;
        GIDTable->segCount = segCount;
        GIDTable->GIDTable = (uint16_t*)malloc(sizeof(uint16_t) * 4 * segCount);
        for(int i = 0; i < 4 ; i++ )
        {   
            if (i == 1) ttffile->ofs += 2;
            
            for (size_t j = 0; j < segCount; j++)
            {
                GIDTable->GIDTable[ i * segCount + j ] = 
                BE16(*(uint16_t*)(ttffile->fileptr + ttffile->ofs + 14 + i * sizeof(uint16_t) * segCount + j * (sizeof(uint16_t))));
            }
        }
    }
    free(encodingRecordSubTable);

#ifdef debug
    printf("映射范围信息：\n");
    for (int j = 0; j < GIDTable->segCount; j++)
    {
                
        printf("(%d,%d)->(%d,%d)\n",
            ((uint16_t (*)[GIDTable->segCount])GIDTable->GIDTable)[1][j], 
            ((uint16_t (*)[GIDTable->segCount])GIDTable->GIDTable)[0][j],
            (int16_t)((uint16_t (*)[GIDTable->segCount])GIDTable->GIDTable)[2][j],
            ((uint16_t (*)[GIDTable->segCount])GIDTable->GIDTable)[3][j]
        );
    }
#endif
}

int get_glyphId(struct GIDTable_info *GIDTable, char character){
    
    uint16_t (*p)[GIDTable->segCount] = (uint16_t (*)[GIDTable->segCount])GIDTable->GIDTable;
    uint16_t character16 = (uint16_t)character;
    int i = 0;
    int glyphId;
    while((int16_t)p[0][i] !=0xFFFF && (int16_t)p[1][i] !=0xFFFF ){
        if ((int16_t)p[1][i] < character16 && (int16_t)p[0][i] > character16)
            break;   
        i++;
    }
    
    if (p[3][i] != 0)
    {
        glyphId = *((int16_t)p[3][i]/2 + (character16 - (int16_t)p[1][i]) + &p[3][i]);
    }else{
        glyphId = (character16 + (int16_t)p[2][i]) & 0xFFFF;
    }
    
#ifdef debug
    printf("glyphId: %d,p[3][i]: %d,i: %d,chara: %d,(%d,%d) \n",glyphId, p[3][i], i, character16,(int16_t)p[1][i],(int16_t)p[0][i]);
#endif

    return glyphId;
}

int get_headTable_info(struct TtfFile* ttffile, struct character_info* info){

    ttffile->ofs = info->head.offset + 50;

    uint16_t indexToLocFormat = BE16(*(uint16_t *)(ttffile->fileptr + ttffile->ofs));

    return indexToLocFormat;
}

void get_loca_index(struct TtfFile* ttffile, struct character_info* Cinfo, struct glyf_info* Ginfo, int glyphId, int indexToLocFormat){

    ttffile->ofs = Cinfo->loca.offset;

#ifdef debug
    printf("indexToLocFormat: %d\n",indexToLocFormat);
#endif

    if(!indexToLocFormat)
    {
        uint16_t *loca_ptr = (uint16_t*)(ttffile->fileptr + ttffile->ofs);
    
        uint32_t current_offset = BE16(loca_ptr[glyphId]) * 2;
        uint32_t next_offset    = BE16(loca_ptr[glyphId + 1]) * 2;
    
        Ginfo->glyf_ofs = current_offset;
        Ginfo->glyf_len = next_offset - current_offset;
    }else{
        uint32_t *loca_ptr = (uint32_t*)(ttffile->fileptr + ttffile->ofs);
        
        uint32_t current_offset = BE32(loca_ptr[glyphId]);
        uint32_t next_offset    = BE32(loca_ptr[glyphId + 1]);
        
        Ginfo->glyf_ofs = current_offset;
        Ginfo->glyf_len = next_offset - current_offset;
    }
    
    #ifdef debug
        printf("字体矢量点偏移与大小\n");
        printf("%d--%d\n",Ginfo->glyf_ofs,Ginfo->glyf_len);
    #endif
}

void get_glyf_point(struct TtfFile* ttffile, struct character_info* Cinfo, struct glyf_info* Ginfo, struct GlyphTable* glyphTable){

    ttffile->ofs = Ginfo->glyf_ofs + Cinfo->glyf.offset;
    glyphTable->numberOfContours =  BE16(*(int16_t*)(ttffile->fileptr + ttffile->ofs));

    if(glyphTable->numberOfContours < 0) return;
    ttffile->ofs += 2;
    
    for (int i = 0; i < 4; i++)
    {
        glyphTable->xyRANGE[i] =  BE16(*(int16_t*)(ttffile->fileptr + ttffile->ofs));
        ttffile->ofs += 2;
    }
    
    glyphTable->endPoints = (uint16_t*)malloc(glyphTable->numberOfContours * sizeof(uint16_t));
    for (int i = 0; i < glyphTable->numberOfContours; i++)
    {
        glyphTable->endPoints[i] = BE16(*(uint16_t*)(ttffile->fileptr + ttffile->ofs));
        ttffile->ofs += 2;
    }

    glyphTable->totalPoints =  glyphTable->endPoints[glyphTable->numberOfContours - 1] + 1;

    int ofs = BE16(*(uint16_t*)(ttffile->fileptr + ttffile->ofs));
    ttffile->ofs += ofs + 2;

    glyphTable->flags = (uint8_t*)malloc(glyphTable->totalPoints * sizeof(uint8_t));
    for(int i = 0; i < glyphTable->totalPoints; i ++){
        glyphTable->flags[i] = *(uint8_t*)(ttffile->fileptr + ttffile->ofs);
        ttffile->ofs+=1;

        if (glyphTable->flags[i] & 0x08)
        {
            uint8_t repeatCount = *(uint8_t*)(ttffile->fileptr + ttffile->ofs);
            ttffile->ofs += 1;
            for (int j = 0; j < repeatCount; j++) {
                if (i < glyphTable->totalPoints) {
                    glyphTable->flags[i+1] = glyphTable->flags[i];
                    if (i == glyphTable->totalPoints)break;
                    i++;
                }
            }
        }
        if (i == glyphTable->totalPoints)break;
    }

    int16_t vector[2] ={0, 0};
    
    glyphTable->xCoords = (uint16_t*)malloc(glyphTable->totalPoints * sizeof(uint16_t));
    for(int i = 0; i < glyphTable->totalPoints; i ++){
        if (glyphTable->flags[i] & 0x02)
        {   
            if (glyphTable->flags[i] & 0x10)
            {
                vector[0] += *(uint8_t*)(ttffile->fileptr + ttffile->ofs);
            }else{
                vector[0] -= *(uint8_t*)(ttffile->fileptr + ttffile->ofs);
            }
            glyphTable->xCoords[i] = vector[0];
            ttffile->ofs+=1;
        }else{

             if (glyphTable->flags[i] & 0x10)
            {
                vector[0] += 0;
            }else{
                vector[0] += BE16((*(int16_t*)(ttffile->fileptr + ttffile->ofs)));
                ttffile->ofs+=2;
            }
            glyphTable->xCoords[i] = vector[0];
        }
    }
    
    glyphTable->yCoords = (uint16_t*)malloc(glyphTable->totalPoints * sizeof(uint16_t));
    for(int i = 0; i < glyphTable->totalPoints; i ++){
        if (glyphTable->flags[i] & 0x04)
        {   
            if (glyphTable->flags[i] & 0x20)
            {
                vector[1] += *(uint8_t*)(ttffile->fileptr + ttffile->ofs);
            }else{
                vector[1] -= *(uint8_t*)(ttffile->fileptr + ttffile->ofs);
            }
            glyphTable->yCoords[i] = vector[1];
            ttffile->ofs+=1;
        }else{

             if (glyphTable->flags[i] & 0x20)
            {
                vector[1] += 0;
            }else{
                vector[1] += BE16((*(int16_t*)(ttffile->fileptr + ttffile->ofs)));
                ttffile->ofs+=2;
            }
            glyphTable->yCoords[i] = vector[1];
        }
    }   

#ifdef debug
    for (size_t i = 0; i < 4; i++)
    {
        printf("%d\n",glyphTable->xyRANGE[i]);
    }

    printf("total:%d, noc:%d\n",glyphTable->totalPoints,glyphTable->numberOfContours);
    
    for (size_t i = 0; i <glyphTable->numberOfContours; i++)
    {
        printf("endPoints :%d\n",glyphTable->endPoints[i]);
    }
    
    
    for (size_t i = 0; i < glyphTable->totalPoints; i++)
    {
        printf("(%d,%d) flag:0x%x\n",glyphTable->xCoords[i],glyphTable->yCoords[i],glyphTable->flags[i]);
    }
    
    
#endif

}

int analysis_TableDirectory(struct TtfFile* ttffile, struct character_info* info){

    struct TableDirectory* TD = (struct TableDirectory*)malloc(sizeof(struct TableDirectory));
    if (TD == NULL)
        return 1;    
    memcpy(TD,ttffile->fileptr,sizeof(struct TableDirectory));
    ttffile->ofs += sizeof(struct TableDirectory);
    TD->sfntVersion   = BE32(TD->sfntVersion);
    TD->numTables     = BE16(TD->numTables);
    TD->searchRange   = BE16(TD->searchRange);
    TD->entryselector = BE16(TD->entryselector);
    TD->rangeShift    = BE16(TD->rangeShift);

#ifdef debug
    printf("0x%x\n",TD->sfntVersion);
    printf("%d\n",TD->numTables);
    printf("%d\n",TD->searchRange);
    printf("%d\n",TD->entryselector);
    printf("%d\n",TD->rangeShift);
#endif

    if(TD->numTables <= 0)
    return 1;

    struct TableRecord* TR= (struct TableRecord*)malloc(sizeof(struct TableRecord) * TD->numTables);

    memcpy(TR,ttffile->fileptr + ttffile->ofs,sizeof(struct TableRecord)* TD->numTables);
    ttffile->ofs += sizeof(struct TableRecord) * TD->numTables;
    for(int i = 0;i < TD->numTables; i++)
    {
        TR[i].checksum = BE32(TR[i].checksum);
        TR[i].offset = BE32(TR[i].offset);
        TR[i].length = BE32(TR[i].length);
#ifdef debug
        printf("Table%d：%.4s\n", i, (char*)&TR[i].tag);
        printf("checksum:0x%x\n",TR[i].checksum);
        printf("offset:%d\n",TR[i].offset);
        printf("length:%d\n",TR[i].length);
        printf("==================================\n");
#endif
    

    if(memcmp(TR[i].tag, "cmap", 4) == 0){memcpy(info->cmap.tag, TR[i].tag, 4); info->cmap.length = TR[i].length; info->cmap.offset = TR[i].offset; info->cmap.checksum = TR[i].checksum;}
    if(memcmp(TR[i].tag, "head", 4) == 0){memcpy(info->head.tag, TR[i].tag, 4); info->head.length = TR[i].length; info->head.offset = TR[i].offset; info->head.checksum = TR[i].checksum;}
    if(memcmp(TR[i].tag, "maxp", 4) == 0){memcpy(info->maxp.tag, TR[i].tag, 4); info->maxp.length = TR[i].length; info->maxp.offset = TR[i].offset; info->maxp.checksum = TR[i].checksum;}
    if(memcmp(TR[i].tag, "loca", 4) == 0){memcpy(info->loca.tag, TR[i].tag, 4); info->loca.length = TR[i].length; info->loca.offset = TR[i].offset; info->loca.checksum = TR[i].checksum;}
    if(memcmp(TR[i].tag, "glyf", 4) == 0){memcpy(info->glyf.tag, TR[i].tag, 4); info->glyf.length = TR[i].length; info->glyf.offset = TR[i].offset; info->glyf.checksum = TR[i].checksum;}
    if(memcmp(TR[i].tag, "hhea", 4) == 0){memcpy(info->hhea.tag, TR[i].tag, 4); info->hhea.length = TR[i].length; info->hhea.offset = TR[i].offset; info->hhea.checksum = TR[i].checksum;}
    if(memcmp(TR[i].tag, "hmtx", 4) == 0){memcpy(info->hmtx.tag, TR[i].tag, 4); info->hmtx.length = TR[i].length; info->hmtx.offset = TR[i].offset; info->hmtx.checksum = TR[i].checksum;}
    
}   

free(TR);
    
#ifdef debug
printf("\n===== 已保存的核心表信息 =====\n");
printf("cmap  -> offset: %-8u length: %-8u\n", info->cmap.offset, info->cmap.length);
printf("head  -> offset: %-8u length: %-8u\n", info->head.offset, info->head.length);
printf("maxp  -> offset: %-8u length: %-8u\n", info->maxp.offset, info->maxp.length);
printf("loca  -> offset: %-8u length: %-8u\n", info->loca.offset, info->loca.length);
printf("glyf  -> offset: %-8u length: %-8u\n", info->glyf.offset, info->glyf.length);
printf("hhea  -> offset: %-8u length: %-8u\n", info->hhea.offset, info->hhea.length);
printf("hmtx  -> offset: %-8u length: %-8u\n", info->hmtx.offset, info->hmtx.length);
printf("==================================\n");
#endif

return 0;
}
    
struct bit_map{
    uint8_t* bit;
    uint16_t x;
    uint16_t y;
};

void set_bit(struct bit_map* bitmap, int x, int y, uint8_t bit){
    if (x>bitmap->x || y>bitmap->y || x<=0 || y<=0)
        return;
    uint8_t ofs = ((x  + bitmap->x * y )%8);
    uint32_t bytes = ((x  + bitmap->x * y )/8);
    bitmap->bit[bytes] = bitmap->bit[bytes] | 1 << (7 - ofs);  
}

uint8_t get_bit(struct bit_map* bitmap, int x, int y){
    if (x>bitmap->x-1 || y>bitmap->y-1 || x<=0 || y<=0)
        return 0;
    uint8_t ofs = ((x  + bitmap->x * y )%8);
    uint32_t bytes = ((x  + bitmap->x * y )/8);
    return (bitmap->bit[bytes] & (1 << (7 - ofs)) ? 1 : 0);  
}

void draw_line_bresenham(struct bit_map* bitmap, int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = -abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy; 

    while (1) {

        set_bit(bitmap, x1, y1, 1);
        
        if (x1 == x2 && y1 == y2) break;
        
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }
}


void draw_line_bezier(struct bit_map* bitmap, int x0, int y0, int x1, int y1, int x2, int y2, int steps) {
    int prev_x = x0;
    int prev_y = y0;

    for (int i = 1; i <= steps; i++) {
        float t = (float)i / steps;
        
        float invT = 1.0f - t;
        float b0 = invT * invT;
        float b1 = 2.0f * t * invT;
        float b2 = t * t;

        int curr_x = (int)(b0 * x0 + b1 * x1 + b2 * x2);
        int curr_y = (int)(b0 * y0 + b1 * y1 + b2 * y2);

        draw_line_bresenham(bitmap, prev_x, prev_y, curr_x, curr_y);

        prev_x = curr_x;
        prev_y = curr_y;
    }
}

void analysis_vector_map(struct character_info* Cinfo, struct GlyphTable* glyphTable, struct bit_map* bitmap){
    
    bitmap->x = glyphTable->xyRANGE[2] - glyphTable->xyRANGE[0];
    bitmap->y = glyphTable->xyRANGE[3] - glyphTable->xyRANGE[1];
    int total_pixels = bitmap ->x * bitmap ->y;
    bitmap->bit = (uint8_t*)malloc((total_pixels+ 7) /8);
    if (!bitmap->bit) exit(-1);
    memset(bitmap->bit, 0, (total_pixels + 7) / 8);
    int start = 0;
    for (size_t i = 0; i < glyphTable->totalPoints; i++)
    {
        glyphTable->xCoords[i] = glyphTable->xCoords[i] - glyphTable->xyRANGE[0];
        glyphTable->yCoords[i] = bitmap->y + 1 - glyphTable->yCoords[i] - glyphTable->xyRANGE[1];
    }
    
    for (size_t nOC = 0; nOC < glyphTable->numberOfContours; nOC++)
    {
        int end = glyphTable->endPoints[nOC];
        for (int  i = start; i <= end; i++)
        {   
            int next1 = (i == end) ? start : i + 1;
            int x1 = glyphTable->xCoords[i];    int y1 = glyphTable->yCoords[i];
            int x2 = glyphTable->xCoords[next1];    int y2 = glyphTable->yCoords[next1];
            
            uint8_t cur_on = glyphTable->flags[i] & 0x01;
            uint8_t nex_on = glyphTable->flags[next1] & 0x01;

            if(cur_on && nex_on){
                //直线
                draw_line_bresenham(bitmap, x1, y1, x2, y2);
            }else if (cur_on && !nex_on)
            {
                //贝塞尔
                int next2 = (next1 == end) ? start : next1 + 1;
                int next3 = (next2 == end) ? start : next2 + 1;
                uint8_t nex2_on = glyphTable->flags[next2] & 0x01;
                uint8_t nex3_on = glyphTable->flags[next3] & 0x01;
                int x3 = glyphTable->xCoords[next2];    int y3 = glyphTable->yCoords[next2];

                
                if (nex2_on)
                {
                    draw_line_bezier(bitmap, x1, y1, x2, y2, x3, y3, 24);
                    i++;
                }else{
                    int xm = (x3 + x2)/2; 
                    int ym = (y3 + y2)/2;
                    draw_line_bezier(bitmap, x1, y1, x2, y2, xm, ym, 24);
                    i+=1;
                }       
            }else if (!cur_on && !nex_on) {
                
                int next2 = (next1 == end) ? start : next1 + 1;
                
                int prev_x = (glyphTable->xCoords[(i == start ? end : i - 1)] + x1) / 2;
                int prev_y = (glyphTable->yCoords[(i == start ? end : i - 1)] + y1) / 2;
                
                int xm = (x1 + x2) / 2;
                int ym = (y1 + y2) / 2;

                draw_line_bezier(bitmap, prev_x, prev_y, x1, y1, xm, ym, 24);

                }
                else if(!cur_on && nex_on){
                    int prev_x = (glyphTable->xCoords[(i == start ? end : i - 1)] + x1) / 2;
                    int prev_y = (glyphTable->yCoords[(i == start ? end : i - 1)] + y1) / 2;

                    draw_line_bezier(bitmap, prev_x, prev_y, x1, y1, x1, y2, 24); 

                }   
            
        }   

    start = end + 1;
    }
    return;
}

void fill_bitmap(struct bit_map* bitmap){

    struct bit_map bitmap_copy;
    bitmap_copy.x = bitmap->x;
    bitmap_copy.y = bitmap->y;

    int total_pixels = bitmap->x * bitmap->y;

    bitmap_copy.bit = (uint8_t*)malloc((total_pixels + 7) / 8);
    if (!bitmap_copy.bit) exit(-1);

    memcpy(bitmap_copy.bit, bitmap->bit, (total_pixels + 7) / 8);

    for (size_t i = 0; i < bitmap->y; i++)
    {
        int8_t flag = 0;
        int8_t cur = 0,pre = 0;
        for (size_t j = 0; j < bitmap->x ; j++)
        {  
            cur = get_bit(bitmap, j, i);
            if(!cur && pre){
                flag = !flag;
            }
            if (flag)
            {
                set_bit(&bitmap_copy,j,i,1);
            }
            pre = cur;
        }
    }
    free(bitmap->bit);
    bitmap->bit = bitmap_copy.bit;
}

//每次长宽减半
void bitmap_half(struct bit_map* bitmap){
    //扫描一个2x2的区域不全就跳过   
    struct bit_map bitmap_resized;
    bitmap_resized.x = (bitmap->x + 1)/2;
    bitmap_resized.y = (bitmap->y + 1)/2;
    int total_pixels = bitmap_resized .x * bitmap_resized.y;
    bitmap_resized.bit = (uint8_t*)malloc((total_pixels+ 7) /8);
    memset(bitmap_resized.bit, 0, (total_pixels + 7) / 8);

    for (size_t i = 0; i < bitmap->y; i+=2)
        {   
            uint8_t x_edge = 1;
            uint8_t y_edge = 1;
            if((i+1) != bitmap->y)y_edge =0;
            for (size_t j = 0; j < bitmap->x; j+=2)
            {
                x_edge = 1;
                if((j+1) != bitmap->x)x_edge =0;
                uint8_t count = 0; 
                if (!x_edge)count +=get_bit(bitmap, j+1, i);
                if (!y_edge)count +=get_bit(bitmap, j, i+1);
                if (!y_edge && !x_edge)count +=get_bit(bitmap, j+1, i+1);
                count +=get_bit(bitmap, j, i);
                if (count >3)
                {
                    set_bit(&bitmap_resized, (j)/2, (i)/2, 1);
                }
                if (x_edge)break;;
            
            }
            if (y_edge && x_edge)break;;
        }
    free(bitmap->bit);
    bitmap->x = bitmap_resized.x;
    bitmap->y = bitmap_resized.y;
    bitmap->bit = bitmap_resized.bit;

}

void bitmap_resize(struct bit_map* bitmap, uint16_t size){
    int x = bitmap->x;
    uint8_t i = 0;
    while (x > size)
    {
        x = x/2;
        i++;
    }
    if (2*x -size < size -x )
    {
        i--;
    }
    while (i)
    {
        bitmap_half(bitmap);
        i--;
    }
}


void bitmap_render(struct bit_map* bitmap){

    char render_ascii[10] = " .-:o#?%&@";
    struct bit_map bitmap_rendered;
    bitmap_rendered.x = (bitmap->x + 2)/3;
    bitmap_rendered.y = (bitmap->y + 2)/3;
    int total_pixels = bitmap_rendered .x * bitmap_rendered.y;
    bitmap_rendered.bit = (uint8_t*)malloc(total_pixels);
    memset(bitmap_rendered.bit,0,total_pixels);
    //扫描一个3x3的区域
    for (size_t i = 0; i < bitmap->y; i+=3)
        {   
            for (size_t j = 0; j < bitmap->x; j+=3)
            {
                uint8_t count = 0;
                count = get_bit(bitmap, j, i) + get_bit(bitmap, j+1, i) + get_bit(bitmap, j+2, i)+
                        get_bit(bitmap, j, i+1) + get_bit(bitmap, j+1, i+1) + get_bit(bitmap, j+2, i+1)+
                        get_bit(bitmap, j, i+2) + get_bit(bitmap, j+1, i+2) + get_bit(bitmap, j+2, i+2);
                 bitmap_rendered.bit[(j/3)+ bitmap_rendered.x*(i/3)] = render_ascii[count];
                 putchar(render_ascii[count]);
                 putchar(render_ascii[count]);
            }
            putchar('\n');
        }
    free(bitmap->bit);
    bitmap->x = bitmap_rendered.x;
    bitmap->y = bitmap_rendered.y;
    bitmap->bit = bitmap_rendered.bit;
}


int main(int argc, char *argv[]) {

    if(argc < 2)exit(1);

    //解析字体文件
    struct TtfFile* ttffile = (struct TtfFile*)malloc(sizeof(struct TtfFile));
    get_tff_file(ttf_path, ttffile);

    struct character_info* info = (struct character_info*)malloc(sizeof(struct character_info));
    analysis_TableDirectory(ttffile, info);

    struct GIDTable_info *GIDTable = (struct GIDTable_info*)malloc(sizeof(struct GIDTable_info));
    get_GlyphTable(ttffile, info, GIDTable);

    int  indexToLocFormat= get_headTable_info(ttffile, info);
    int glyphId = get_glyphId(GIDTable, argv[1][0]);
    free(GIDTable->GIDTable);
    free(GIDTable);

    struct glyf_info* Ginfo = (struct glyf_info*)malloc(sizeof(struct glyf_info));
    get_loca_index(ttffile, info, Ginfo, glyphId, indexToLocFormat);

    struct GlyphTable* glyphTable = (struct GlyphTable*)malloc(sizeof(struct GlyphTable));

    get_glyf_point(ttffile, info, Ginfo, glyphTable);
    
    //对解析出来的数据进行渲染
    struct bit_map* bitmap = (struct bit_map*)malloc(sizeof(struct bit_map));
    analysis_vector_map(info, glyphTable, bitmap);

    free(ttffile->fileptr);
    free(ttffile);

    free(info);

    free(Ginfo);

    free(glyphTable->xCoords);
    free(glyphTable->yCoords);
    free(glyphTable->flags);
    free(glyphTable->endPoints);
    free(glyphTable);
    fill_bitmap(bitmap);
    bitmap_resize(bitmap,(uint16_t)atoi(argv[2]));
    bitmap_render(bitmap);

    free(bitmap);
    return 0;
}