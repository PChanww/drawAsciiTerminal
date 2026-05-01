/**
 * @file drawAsciiTerminal.c
 * @author PChanww
 * @date 2026-05-01
 * @version 1.0
 * @brief TTF font parser and bitmap renderer 
 *        still lot of bugs
 *
 * Copyright (c) 2026 PChanww
 *
 * Licensed under the MIT License.
 * See LICENSE file in the project root for full license information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// #define debug

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
    uint8_t *encodingRecordSubTable = (uint8_t*)malloc(8 * numTables);

    ttffile->ofs = info->cmap.offset + 2;
    memcpy(&numTables,ttffile->fileptr + ttffile->ofs,2);
    numTables = BE16(numTables);

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

    printf("indexToLocFormat: %d\n",indexToLocFormat);

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
    

struct Bitmap {
    int width;
    int height;
    uint8_t *data;
};

void bitmap_init(struct Bitmap *bitmap, int width, int height) {
    bitmap->width = width;
    bitmap->height = height;
    bitmap->data = (uint8_t *)calloc(width * height, sizeof(uint8_t));
}

void bitmap_free(struct Bitmap *bitmap) {
    if (bitmap->data) {
        free(bitmap->data);
        bitmap->data = NULL;
    }
}

int sign(int x) {
    if (x > 0) return 1;
    if (x < 0) return -1;
    return 0;
}

void bitmap_draw_line(struct Bitmap *bitmap, int x0, int y0, int x1, int y1, uint8_t intensity) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = sign(x1 - x0);
    int sy = sign(y1 - y0);
    int err = dx - dy;
    int x = x0, y = y0;

    while (1) {
        if (x >= 0 && x < bitmap->width && y >= 0 && y < bitmap->height) {
            bitmap->data[y * bitmap->width + x] += intensity;
            if (bitmap->data[y * bitmap->width + x] > 255) 
                bitmap->data[y * bitmap->width + x] = 255;
        }
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

void bitmap_fill_contour(struct Bitmap *bitmap, struct GlyphTable *glyph, int16_t minX, int16_t minY, float scale) {
    int contourStart = 0;
    
    for (int c = 0; c < glyph->numberOfContours; c++) {
        int contourEnd = glyph->endPoints[c];
        
        for (int i = contourStart; i <= contourEnd; i++) {
            int nextI = (i == contourEnd) ? contourStart : (i + 1);
            
            int x0 = (int)((glyph->xCoords[i] - minX) * scale);
            int y0 = (int)((glyph->yCoords[i] - minY) * scale);
            int x1 = (int)((glyph->xCoords[nextI] - minX) * scale);
            int y1 = (int)((glyph->yCoords[nextI] - minY) * scale);
            
            bitmap_draw_line(bitmap, x0, y0, x1, y1, 80);
        }
        
        contourStart = contourEnd + 1;
    }
    
    // Improved flood fill from inside
    for (int y = 0; y < bitmap->height; y++) {
        int inFill = 0;
        int lastBoundary = -10;
        
        for (int x = 0; x < bitmap->width; x++) {
            if (bitmap->data[y * bitmap->width + x] > 40) {
                if (inFill == 0 && x - lastBoundary > 2) {
                    inFill = 1;
                    lastBoundary = x;
                } else if (inFill == 1) {
                    inFill = 0;
                    lastBoundary = x;
                }
            }
        }
        
        inFill = 0;
        for (int x = 0; x < bitmap->width; x++) {
            if (bitmap->data[y * bitmap->width + x] > 40) {
                inFill = (inFill + 1) % 2;
            } else if (inFill == 1 && bitmap->data[y * bitmap->width + x] <= 40) {
                bitmap->data[y * bitmap->width + x] = 160;
            }
        }
    }
}

char intensity_to_ascii(uint8_t intensity) {
    const char *ramp = " .:-=+*#%@";
    int level = (intensity * 10) / 256;
    if (level > 9) level = 9;
    return ramp[level];
}

void bitmap_render_ascii(struct Bitmap *bitmap, int charWidth, int charHeight) {
    int cellWidth = bitmap->width / charWidth;
    int cellHeight = bitmap->height / charHeight;
    
    if (cellWidth <= 0) cellWidth = 1;
    if (cellHeight <= 0) cellHeight = 1;
    
    for (int y = charHeight - 1; y >= 0; y--) {
        for (int x = 0; x < charWidth && x * cellWidth < bitmap->width; x++) {
            uint32_t sum = 0;
            int count = 0;
            
            for (int dy = 0; dy < cellHeight && y * cellHeight + dy < bitmap->height; dy++) {
                for (int dx = 0; dx < cellWidth && x * cellWidth + dx < bitmap->width; dx++) {
                    sum += bitmap->data[(y * cellHeight + dy) * bitmap->width + (x * cellWidth + dx)];
                    count++;
                }
            }
            
            uint8_t avgIntensity = (count > 0) ? (sum / count) : 0;
            putchar(intensity_to_ascii(avgIntensity));
        }
        putchar('\n');
    }
}

int main(int argc, char *argv[]) {

    char character = 'A';
    int renderSize = 64;

    if (argc > 1) {
        character = argv[1][0];
    }
    if (argc > 2) {
        renderSize = atoi(argv[2]);
        if (renderSize < 8) renderSize = 8;
        if (renderSize > 256) renderSize = 256;
    }

    // Parse TTF file
    struct TtfFile* ttffile = (struct TtfFile*)malloc(sizeof(struct TtfFile*));
    if (get_tff_file(ttf_path, ttffile) != 0) {
        fprintf(stderr, "Failed to load TTF file\n");
        return 1;
    }

    struct character_info* info = (struct character_info*)malloc(sizeof(struct character_info));
    if (analysis_TableDirectory(ttffile, info) != 0) {
        fprintf(stderr, "Failed to parse table directory\n");
        return 1;
    }

    struct GIDTable_info *GIDTable = (struct GIDTable_info*)malloc(sizeof(struct GIDTable_info));
    get_GlyphTable(ttffile, info, GIDTable);

    int indexToLocFormat = get_headTable_info(ttffile, info);
    int glyphId = get_glyphId(GIDTable, character);
    free(GIDTable);

    struct glyf_info* Ginfo = (struct glyf_info*)malloc(sizeof(struct glyf_info));
    get_loca_index(ttffile, info, Ginfo, glyphId, indexToLocFormat);

    struct GlyphTable* glyphTable = (struct GlyphTable*)malloc(sizeof(struct GlyphTable));
    get_glyf_point(ttffile, info, Ginfo, glyphTable);

    // Rasterize and render
    int16_t minX = glyphTable->xyRANGE[0];
    int16_t minY = glyphTable->xyRANGE[1];
    int16_t maxX = glyphTable->xyRANGE[2];
    int16_t maxY = glyphTable->xyRANGE[3];
    
    int glyphWidth = maxX - minX;
    int glyphHeight = maxY - minY;
    
    if (glyphWidth <= 0 || glyphHeight <= 0) {
        fprintf(stderr, "Invalid glyph dimensions\n");
        return 1;
    }

    float aspectRatio = (float)glyphWidth / glyphHeight;
    int bitmapWidth = renderSize;
    int bitmapHeight = (int)(renderSize / aspectRatio);
    if (bitmapHeight < 4) bitmapHeight = 4;

    float scale = (float)bitmapWidth / glyphWidth;

    struct Bitmap bitmap;
    bitmap_init(&bitmap, bitmapWidth, bitmapHeight);
    bitmap_fill_contour(&bitmap, glyphTable, minX, minY, scale);

    int charWidth = (bitmapWidth + 7) / 8;
    int charHeight = (bitmapHeight + 3) / 8;
    bitmap_render_ascii(&bitmap, charWidth, charHeight);

    bitmap_free(&bitmap);
    free(ttffile->fileptr);
    free(ttffile);
    free(info);
    free(Ginfo);
    free(glyphTable->endPoints);
    free(glyphTable->flags);
    free(glyphTable->xCoords);
    free(glyphTable->yCoords);
    free(glyphTable);

    return 0;
}