/*
	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
//to compile type gcc -Wall -Wextra -lm -lpng -O2 -o yuv main.c
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include <zlib.h>
uint32_t img_w=640;
uint32_t img_w_2=1280;
uint32_t img_h=480;
uint32_t img_wo=640;
uint32_t img_ho=480;
#define CLIP(X) ( (X) > 255 ? 255 : (X) < 0 ? 0 : X)
#define C(Y) ( (Y) - 16  )
#define D(U) ( (U) - 128 )
#define E(V) ( (V) - 128 )
#define YUV2R(Y, U, V) CLIP(( 298 * C(Y)              + 409 * E(V) + 128) >> 8)
#define YUV2G(Y, U, V) CLIP(( 298 * C(Y) - 100 * D(U) - 208 * E(V) + 128) >> 8)
#define YUV2B(Y, U, V) CLIP(( 298 * C(Y) + 516 * D(U)              + 128) >> 8)
char buf[1024];
void showHelp()
{
	puts("Yuv422/raw image data to png");
	puts("-n x replace x with the image number you want to convert");
	puts("-h or --help shows this help file");
	puts("-o x replace x with a real integer between 0 and 7 this sets the offset");
	puts("-a x replace x with the amount of frames that you wish to averge don't use this if you don't want to average frames");
	puts("-c picks which algorthim you would like to use you can specify either 'y' or 'd' or 'dq'\ny means yuv422 conversion and d means to debayer by default debayering conversion is used dq means to take debayered data and output quater resolution but it does not to any interopulation instead it takes the 4 pixels and makes one\ndn means use neighest neighboor debayer instead of bilinear");
	puts("-w specifies width (defaults to 640)");
	puts("-H specifies height (defaults to 480)");
}
int savePNG(char * fileName,uint32_t width,uint32_t height,void * ptr)
{
	//saves a 24bit png with rgb byte order
	png_byte * dat=ptr;//convert to uint8_t
	//png_byte **row_pointers = malloc(height*sizeof(png_byte));
	//if (row_pointers==0)
	//	return 1;
	FILE * fp=fopen(fileName,"wb");
	if (fp==0)
		return 1;
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)0,0,0);
	if (!png_ptr)
		return 1;
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr){
		png_destroy_write_struct(&png_ptr,(png_infopp)NULL);
		return 1;
	}
	if (setjmp(png_jmpbuf(png_ptr))){
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return 1;
	}
	png_init_io(png_ptr, fp);
	png_set_IHDR(png_ptr, info_ptr, width, height,8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);//must be called before other png_set_*() functions
	png_set_compression_level(png_ptr,Z_BEST_COMPRESSION);
	uint32_t y;
	png_set_user_limits(png_ptr, width, height);
	png_write_info(png_ptr, info_ptr);
	for (y=0;y<height;y++)
		png_write_row(png_ptr, &dat[(y*width*3)]);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);//done with file
	return 0;//will return 0 on success non-zero in error
}
void yuv2rgb(uint8_t * yuvDat,uint8_t * out)
{
	uint64_t xy;
	for (xy=0;xy<(img_w/2)*img_h;xy++){
		*out++=YUV2R(yuvDat[0],yuvDat[1],yuvDat[3]);
		*out++=YUV2G(yuvDat[0],yuvDat[1],yuvDat[3]);
		*out++=YUV2B(yuvDat[0],yuvDat[1],yuvDat[3]);
		*out++=YUV2R(yuvDat[2],yuvDat[1],yuvDat[3]);
		*out++=YUV2G(yuvDat[2],yuvDat[1],yuvDat[3]);
		*out++=YUV2B(yuvDat[2],yuvDat[1],yuvDat[3]);
		yuvDat+=4;
	}
}
void deBayerBL(uint8_t * in,uint8_t * out)
{
	uint32_t x,y;
	for (y=0;y<img_h*img_w;y+=2*img_w){
		for (x=0;x<img_w;x+=2){
			/*	B Gb
				Gr R*/
			
			if(y!=0){
				if(x!=0)
					out[x*3]=(in[y+x+1+img_w]+in[y+x+1-img_w]+in[y+x-1+img_w]+in[y+x-1-img_w])/4;//red
				else
					out[x*3]=(in[y+x+1+img_w]+in[y+x+1-img_w])/2;//red
			}else{
				if(x!=0)
					out[x*3]=(in[y+x+1+img_w]+in[y+x-1+img_w])/2;//red
				else
					out[x*3]=in[y+x+1+img_w];//red
			}
			if(x!=0)//edges need special handling
				out[(x*3)+1]=(in[y+x-1]+in[y+x+1])/2;//green
			else
				out[(x*3)+1]=in[y+x+1];//green
			out[(x*3)+2]=in[y+x];//blue
			uint8_t r,b;
			if(y!=0)
				r=(in[y+x+1+img_w]+in[y+x+1-img_w])/2;//red
			else
				r=in[y+x+1+img_w];
			out[(x*3)+3]=r;
			out[(x*3)+4]=in[y+x+1];//green
			b=(in[y+x]+in[y+x+img_w_2])/2;
			out[(x*3)+5]=b;//blue
			
			out[((x+img_w)*3)]=r;
			out[((x+img_w)*3)+1]=in[y+x+img_w];
			out[((x+img_w)*3)+2]=b;
			
			out[((x+img_w)*3)+3]=in[y+x+1+img_w];//red
			out[((x+img_w)*3)+4]=in[y+x+img_w];//green
			if(y!=0){//blue
				out[((x+img_w)*3)+5]=(in[y+x+2-img_w_2]+in[y+x-img_w_2]+in[y+x+2+img_w_2]+in[y+x+img_w_2])/4;
			}else{
				out[((x+img_w)*3)+5]=(in[y+x+2+img_w_2]+in[y+x+img_w_2])/2;
			}
		}
		out+=img_w*6;
		//in+=img_w_2;
	}
	
}
void deBayerSSDD(uint8_t * in,uint8_t * out)
{//from http://www.ipol.im/pub/art/2011/bcms-ssdd/
	uint32_t x,y;
	for (y=0;y<img_h;y+=2){
		for (x=0;x<img_w;x+=2){
			/*B Gb
			  Gr R*/
			out[x*3]=in[x];
		}
	}
	
}
void deBayerQ(uint8_t * in,uint8_t * out)
{//generates quater resolution but pixel has real RGB value at each location
	uint32_t x,y;
	for (y=0;y<img_ho;++y){
		for (x=0;x<img_w;x+=2){
			out[(x/2*3)]=in[x+1+img_w];//red
			out[(x/2*3)+1]=(in[x+1]+in[x+img_w])/2;//green
			out[(x/2*3)+2]=in[x];//blue
		}
		out+=img_wo*3;
		in+=img_w*2;
	}
}
void deBayerN(uint8_t * in,uint8_t * out)
{
	uint32_t x,y;
	for (y=0;y<img_h;y+=2){
		for (x=0;x<img_w;x+=2){
			/* Correct table: (It is slightly different than the most common R G G B tables)
			 B Gb
			 Gr R
			*/ 
			out[(x*3)]=in[x+1+img_w];//red 
			out[(x*3)+1]=in[x+1];//green
			out[(x*3)+2]=in[x];//blue
			
			out[(x*3)+3]=in[x+1+img_w];//red 
			out[(x*3)+4]=in[x+1];//green		
			out[(x*3)+5]=in[x];//blue

			out[((x+img_w)*3)]=in[x+1+img_w];//red 
			out[((x+img_w)*3)+1]=in[x+img_w];//green
			out[((x+img_w)*3)+2]=in[x];//blue

			out[((x+img_w)*3)+3]=in[x+1+img_w];//red 
			out[((x+img_w)*3)+4]=in[x+img_w];//green
			out[((x+img_w)*3)+5]=in[x];//blue
		}
		out+=img_w*6;
		in+=img_w*2;
	}
}
uint8_t readImg(uint32_t numf,uint16_t offset,uint8_t * dat,uint8_t alg)
{
	if(alg!=0)
		sprintf(buf,"F%d.RAW",numf);
	else
		sprintf(buf,"F%d.YUV",numf);
	FILE * myfile = fopen(buf,"rb");
	if (myfile==0){
		printf("Cannot open file %s\n",buf);
		return 1;
	}
	if (offset!=0)
		fseek(myfile,offset,SEEK_SET);
	int error=0;
	if (alg != 0)
		error=fread(dat,1,(img_w*img_h)-offset,myfile);
	else
		error=fread(dat,1,(img_w*img_h*2)-offset,myfile);
	fclose(myfile);
	if(error==0){
		puts("Error read 0 bytes");
		exit(1);
	}
	return 0;
}
uint8_t processImg(uint8_t * in,uint8_t * out,uint32_t numf,uint8_t alg,uint16_t offset)
{
	if (readImg(numf,offset,in,alg))
		return 1;
	switch (alg){
	case 3:
		deBayerBL(in,out);
	break;
	case 2:
		deBayerQ(in,out);//causes low resolution but it's like have a 3cmos sensor or foveon sensor
	break;
	case 1:
		deBayerN(in,out);//nearest neighboor low quality but fast
	break;
	case 0:
		yuv2rgb(in,out);
	break;
	default:
		puts("You must pick a valid algorithm to save the image as");
		return 1;
	}
	return 0;
}
void avgF(uint16_t numf,uint8_t * inout)
{
	uint16_t * temp=malloc(img_w*img_h*3*numf*sizeof(uint16_t));
	uint64_t xy;
	uint64_t nl;
	for (xy=0;xy<img_w*img_h*3;xy++)
		temp[xy]=inout[xy];
	for (nl=img_w*img_h*3;nl<numf*img_w*img_h*3;nl+=img_w*img_h*3){
		//printf("Adding %d\n",nl/img_w/img_h/3);
		for (xy=0;xy<=img_w*img_h*3;xy++)
			temp[xy]+=inout[xy+nl];
	}
	for (xy=0;xy<img_w*img_h*3;xy++)
		temp[xy]/=numf;
	for (xy=0;xy<img_w*img_h*3;xy++)
		inout[xy]=temp[xy];
	free(temp);
}
int main(int argc,char ** argv)
{
	uint8_t useNum=0;
	uint32_t useImg=0;
	uint16_t offset=0;
	uint8_t debayer=3;
	uint16_t numImg=1;
	if (argc>1){
		//handle arguments
		int arg;
		for (arg=0;arg<argc;arg++){
			if (strcmp(argv[arg],"-c") == 0){
				arg++;
				if(strcmp(argv[arg],"y")==0)
					debayer=0;
				else if(strcmp(argv[arg],"d")==0)
					debayer=3;
				else if(strcmp(argv[arg],"dq")==0)
					debayer=2;
				else if(strcmp(argv[arg],"dn")==0)
					debayer=1;
				else{
					puts("You did not specify a valid algorithm See usage (below)");
					showHelp();
					return 1;
				}
				continue;
			}
			if (strcmp(argv[arg],"-w") == 0){
				arg++;
				img_wo=img_w=atoi(argv[arg]);
				img_w_2=img_w+img_w;
				continue;
			}
			if (strcmp(argv[arg],"-H") == 0){
				arg++;
				img_ho=img_h=atoi(argv[arg]);
				continue;
			}
			if (strcmp(argv[arg],"-n") == 0){
				arg++;
				useImg=atoi(argv[arg]);
				if (useNum != 2)
					useNum=1;
				continue;
			}
			if (strcmp(argv[arg],"-o") == 0){
				arg++;
				offset=atoi(argv[arg]);
				continue;
			}
			if (strcmp(argv[arg],"-a") == 0){
				arg++;
				useNum=2;
				numImg=atoi(argv[arg]);
				if (numImg <1){
					printf("For argument -a you must specify a number greater than 0 you entered %d\n",numImg);
					return 1;
				}
				continue;
			}
			if ((strcmp(argv[arg],"-h")==0) || (strcmp(argv[arg],"--help")==0)){
				showHelp();
				return 0;
			}
		}
	}
	uint8_t * Dat;//in case some of the file was not saved we use calloc instead of malloc to garentte that the unsaved pixels are set to 0
	if (debayer!=0)
		Dat = calloc(img_w*img_h,1);
	else
		Dat = calloc(img_w*img_h,2);
	if(debayer==2){
		img_wo/=2;
		img_ho/=2;
	}
	uint8_t * outImg = malloc(img_wo*img_ho*numImg*3);//all bytes in the array will be overwritten no need for calloc
	if (useNum==1){
		processImg(Dat,outImg,useImg,debayer,offset);
		sprintf(buf,"frame %d.png",useImg);
		if (savePNG(buf,img_wo,img_ho,outImg)){
			puts("Error while saving PNG");
			return 1;
		}
	}
	else if (useNum==2){
		uint32_t nl;
		for (nl=0;nl<numImg;nl++){
			printf("Reading %d\n",nl);
			processImg(Dat,outImg+(nl*img_wo*img_ho*3),useImg+nl,debayer,offset);
		}
		avgF(numImg,outImg);
		sprintf(buf,"frame %d-%d.png",useImg,useImg+numImg-1);
		if (savePNG(buf,img_wo,img_ho,outImg)){
			puts("Error while saving PNG");
			return 1;
		}
	}else{
		uint32_t imgC=0;
		for (;;imgC++){
			printf("Saving image %d\n",imgC);
			if (processImg(Dat,outImg,imgC,debayer,offset))
				goto quit;
			sprintf(buf,"frame %d.png",imgC);
			if (savePNG(buf,img_wo,img_ho,outImg)){
				puts("Error while saving PNG");
				return 1;
			}
		}
	}
quit:
	free(Dat);
	free(outImg);
	return 0;
}
