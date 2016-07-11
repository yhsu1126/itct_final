#include "common.h"

using namespace std;

struct node{
	int value;
	bool isLeaf;
	node* left;
	node* right;
};

struct pixel{
	uint8 r;
	uint8 g;
	uint8 b;
};


struct MCU{
	int y[4][8][8]; // 0~ 3
	int cb[8][8]; //5
	int cr[8][8]; // 4
};

struct pel{
	int y;
	int cb;
	int cr;
};

struct frame{
	pixel* bitmap;
};

struct video{
	int horizontalSize; // 12 bits
	int verticalSize; // 12 bits
	int mb_col; // for other calculation of macroblock amount
	int mb_row; // for other calculation
	double pel_aspect_ratio; // a 4 bits index to check table and get value
	double picture_rate;// a 4 bits index to check table and get value
	int bit_rate; // 18 bits bit rate
	int markerBit; // 1 bit check flag
	int vbvBufferSize; // 10 bits
	int constrained_parameters_flag; // 1 bit
	int load_intra_quantizer; // 1 bit
	uint8 intra_quantizer[8][8]; // 8 bit * 64
	int load_non_intra_quantizer; // 1 bit
	uint8 non_intra_quantizer[8][8]; // 8 bits * 64
	vector<uint8> extension_data; // 8 bit
	vector<uint8> user_data; // 8 bit
	vector<gop> gops;
	vector<picture>forward_frame;
	vector<picture>backward_frame;
};

struct gop{
	uint8 drop_frame_flag; // 1 bit;
	uint8 time_code_hours; // 5 bit;
	uint8 time_code_mins; // 6 bit;
	uint8 markerBit; // 1 bit;
	uint8 time_code_secs; // 6 bit;
	uint8 time_code_pics; // 6 bit;
	uint8 closed_gop; //1 bit flag;
	uint8 broken_link;//1 bit flag;
	vector<uint8> extension_data;
	vector<uint8> user_data;
	map<int,picture>pictures;
};

struct picture{
	int temporal_reference;// the display order ? 10 bits
	uint8 picture_coding_type;// 3 bits
	int vbv_delay;//	16 bits
	int previousBlockAddress; // the previous block address for the 
	uint8 full_pel_forward_vector;// 1 bit for P type or B type picture
	uint8 forward_f_code; // 3 bit
	int forward_r_size;
	int forward_f;
	
	//forward motion Vector stuff
	int recon_right_for_prev;
	int recon_down_for_prev;
	// backward motion vector stuff
	int recon_right_back_prev;
	int recon_down_back_prev;
	
	uint8 full_pel_backward_vector; // 1 bit for B type picture only
	uint8 backward_f_code; // 3 bit
	int backward_r_size;
	int backward_f;
	uint8 extra_bit;// 1 bit if set to 1 then there's extra information
	vector<uint8> extra_information; // 8 bits
	vector<uint8> extension_data; 
	vector<uint8> user_data;
	vector<slice> slices;
	int **bitmap;
	pel **pelmap;	
};

struct slice{
	int verticalPos;//8 bit
	uint8 scale;// 5 bits
	uint8 extra_bit;// 1 bit
	vector<uint8> extra_information; // 8 bit
	vector<macroblock> macroBlocks;
};

struct macroblock{
	int type;
	int address;
	int macroblock_quant;
	int macroblock_motion_forward;
	int macroblock_motion_backward;
	int macroblock_pattern;
	int macroblock_intra;
	uint8 quant_scale;
	int motion_horizontal_forward_code;
	int motion_horizontal_forward_r;
	int recon_right_for;
	int recon_right_back;
	int motion_vertical_forward_code;
	int motion_vertical_forward_r;
	int recon_down_for;
	int recon_down_back;
	int motion_horizontal_backward_code;
	int motion_horizontal_backward_r;
	int motion_vertical_backward_code;
	int motion_vertical_backward_r;
	int pattern_code;
	mcu myMCU;
};

char  fileName[500];
uint8 *buffer = NULL;
uint8 myByte;
FILE *my_fd;
FILE *tmpfd; // this occasion is really rare to happen, we just open a new fd to the current fd place and read the stuff out
int bufferSize = 200000,inputIndex,byteIndex;
int pictureIndex=0;
vector<frame>AllBuffer;
bool readytoDraw = false;
bool finishDecode = false;
bool isPaused = false;
bool seeking = false;
int seekFrame = 0;
pixel *blackScreen;

video myVideo;
uint8 predictionVector[3];
node *macroblockAddressIncRoot=(node*)malloc(sizeof(node));
node *macroblockTypeRoot = (node*)malloc(sizeof(node)*4);
node *motionVectorRoot = (node*)malloc(sizeof(node)); // the table to decode motion vector horizontal backward forward vertical backward forward code
node *macroblockPatternRoot = (node*)malloc(sizeof(node));
node *blockSizeLuminanceRoot = (node*)malloc(sizeof(node));
node *blockSizeChrominanceRoot = (node*)malloc(sizeof(node));
node *dcCoefRoot = (node*)malloc(sizeof(node));

void buildAllVLC();//build all vlc table for the MPEG deoder
void testVLC(node*); // debug function test a root node
inline int huffmanDecode(node*); //thefunction to decode the huffman code given the root node of the binary tree

inline int readInt();	 //read 4 byte, but I don't thin I use it now
inline uint8 readByte(); //read a byte
inline void findHeader(); // basically the next start code
inline int read3Byte(); //read 3 byte.. basiallly the one for header finding
inline int readnBytes(int); //read n bytes
inline int readBits(); //read a bits
inline void clear_Bits(); // the byte aligned function in the spec
inline void readSkip(int);//skip some part of the buffer
inline void initializeNode(node*); //initialize a binary tree node
inline int peaknBits(int); // basically this is the nextnBits inside the  spec
inline int peaknBytes(int); // basically a version where use byte is much faster then peaknBits(32);
inline int readnBits(int); //read n bits from the buffer
int returnSign(int); //return -1 if less than 0 , 1 if larger than 0 and 0 for 0;


inline void toDCFirst(); // make the dct coefficient binary tree of huffman code to dc first type. of the value 11
inline void toDCNext(); // make the dct coefficient binary tree of huffman code to dc Next type of the value 11

void initFile(); //init file handle the buffer
inline void readFile(); //read 1 byte from the file, this is for next bit predition when the content we want is not in the buffer
long fileSize,currentBufferSize;

void video_sequence(); //function to parse the video layer
void readSequenceHeader(); //function to parse the sequencelayer
void decode_gop(gop*); //function to parse the GOP layer
void decode_picture(picture*); //function to parse the picture layer
void decode_slice(slice*,picture*); //function to parse the slice layer
void decode_macroblock(macroblock*,slice*,picture*); //function to parse macrobloc layer
void decode_block(macroblock*, int, picture*); //the funtion to parse block layer;
void reconstructPictureFromSkippedMacroBlock(picture*, int, slice*); //the funtion to handle skipped macroblock
void doMotionCompensation(picture*, macroblock*); // do the motion compensation part to copy a block of rgb value to the target place
void calculateMotionVector(picture*, macroblock*); //caculate the motion vector of the macroblok ForMV and BackMV

inline void copyPel(picture*,picture*,int,int,int,int);// copy the pel value of the picture to the current pel pel
inline void initializePel(picture*,int,int); // initialize all the value to 0
inline void dividePel(picture*,int,int,float); //divide the pel value and round them with a number

inline void simpleCopyPel(pel**,picture*,int,int); // copy the pel value of the picture to the current pel pel
inline void simpleInitializePel(pel**); // initialize all the value to 0
inline void simpleDividePel(pel**,float); //divide the pel value and round them with a number


void travelTree(node*,int,int); // debug function for binary tree
void travelDCTree(node*,int,int); // debug funtion for binary tree
void buildHuffman(node*,pair<const char*,int>*,int); // build the huffman binary tree from the value in constant.h

//void outputGOP(gop*);
void decodePictureRGB(picture*); //change the picture from dct coefficient to Y CB CR
inline void outputPicture(picture); // output the picture to display
static void idct1(int*, int[8][8],int, int, int); // the 1 dimension fast IDCT
void idct(int [8][8]);// fast IDCT
inline int YCbCrtoRGB(double, double,double); // y cb cr to rgb
void output_bmp(string ,int**); //debug funtion
void output_bmp2(string ,pixel**); //debug funtion 2
void debugBlock(int [8][8]); //debug function to debug the block

//gui function
void display(){
	 //cout<<"going to display "<<pictureIndex<<endl;
	//string title(pictureName);
	 //output_bmp2(title,(AllBuffer.at(pictureIndex).bitmap));
	 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
     glDrawPixels(myVideo.horizontalSize, myVideo.verticalSize, GL_RGB, GL_UNSIGNED_BYTE, (AllBuffer.at(pictureIndex)).bitmap);
	 glutSwapBuffers();
}

// the func to drawn the screen to all black;
void resetScreen(){ //the funtion to change the screen to whole black
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawPixels(myVideo.horizontalSize, myVideo.verticalSize, GL_RGB, GL_UNSIGNED_BYTE, blackScreen);
	glutSwapBuffers();
}

void fetchFrame(int value){ // the funtion to swap the frame for the display funtion
	if(isPaused){
		return;
	}
	int tmp = value;
	if(seeking){
		tmp = seekFrame;
		seeking = false;
	}
	if(value < AllBuffer.size()){
		pictureIndex = value;
		tmp ++;
		glutPostRedisplay();
	}
	else if (finishDecode == true && value >= AllBuffer.size()){
		tmp = 0;
	}
	//cout<<"going to fetch "<<tmp<<endl;
	glutTimerFunc(1000 / myVideo.picture_rate, fetchFrame, tmp);
}

void inputKey(unsigned char key, int xmouse, int ymouse){ // get the input key
    if(key == 'p' || key == 'P'){
    	if(isPaused == true){
    		glutTimerFunc(1000/myVideo.picture_rate,fetchFrame,pictureIndex);
    	}
    	isPaused = !isPaused;
    }
    else if(key == 'q' || key == 'Q'){
    	exit(0);
    }
    else if(key =='s' || key == 'S'){
    	pictureIndex = 0;
    	isPaused = true;
    	resetScreen();
    }
    else{
    	cout<<"UNknown Key O  WO :  "<<key<<endl;
    }
}


void constructBlackScreen(){ // construt the black screen for the gui
	blackScreen = (pixel*)malloc(sizeof(pixel)*myVideo.verticalSize*myVideo.horizontalSize);
	for(int i=0;i<myVideo.verticalSize*myVideo.horizontalSize;i++){
		blackScreen[i].r = 0;
		blackScreen[i].g = 0;
		blackScreen[i].b = 0;
	}
}

void OnMouseClick(int button, int state, int x, int y)
{
  if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)  // try to seek the frame that I want to play
  { 
     //store the x,y value where the click happened
     //puts("Middle button clicked");
	 seekFrame = (int)round((float)AllBuffer.size()*((float)(x)/(float)(myVideo.horizontalSize)));
	 seeking = true;
	 if(isPaused == true){
		 seeking = false;
		 pictureIndex = seekFrame;
		 display();
	 }
  }	
}
//end of gui functions
int main(int argc, char* argv[]){
	buildAllVLC();
	if(argc >=2){
		strcat(fileName,argv[1]);
	}
	else{
		strcat(fileName,"I_ONLY.M1V");
	}
	initFile();
	//open a thread to decode
	thread decode_thread(video_sequence); // add the stuff to one thread to do the decode stuff O  WO
	while(readytoDraw == false){
		//cout<<"going to sleep 0.0/"<<endl;
		usleep(2000);
	}
	//cout<<"I got the Size "<<myVideo.horizontalSize<<"  "<<myVideo.verticalSize<<endl;
	constructBlackScreen(); // the black screen for the GUI
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(myVideo.horizontalSize, myVideo.verticalSize); 
	glutCreateWindow("itct Final project");
	glutDisplayFunc(display);
	glutKeyboardFunc(inputKey);
	glutTimerFunc(1000/myVideo.picture_rate,fetchFrame,0);
	glutMouseFunc(OnMouseClick);
	glutMainLoop();
	//video_sequence(); //decode the video sequence layer
	decode_thread.join();
	return 0;
}

void video_sequence(){ // basically read the whole video
	findHeader();
	while(peaknBytes(4) == sequence_header_code){
		readSkip(4);
		readSequenceHeader();
		while(peaknBytes(4) == group_start_code){
			gop myGOP;
			decode_gop(&myGOP);
			myVideo.gops.push_back(myGOP);
		}
	}
	if(peaknBytes(4) == sequence_end_code){
		//cout<<"End Detect: decode finish"<<endl;
		if(myVideo.backward_frame.size()!=0){
			outputPicture(myVideo.backward_frame.back());
		}
		finishDecode = true;
	}
}

inline uint8 readByte(){
	if(inputIndex >= currentBufferSize){
		readFile();
		inputIndex = 0;
	}
	uint8 tmp = buffer[inputIndex];
	inputIndex = inputIndex +1;
	return tmp;
}

inline int read3Byte(){
	int result = 0;
	result = result | (int)readByte();
	for(int i=0;i<2;i++){
		result = result<<8;
		result = result | (int)readByte();
	}
	return result;
}

inline void findHeader(){ //get the next start code;
	int flag=0; // need to check for 1 in the next three byte
	clear_Bits();
	while(peaknBytes(3) != 1)
	{
		readSkip(1);
	}
}

inline int readBits(){
	if(byteIndex ==0){
		myByte = readByte();
		byteIndex = 8;
	}
	
	int result = (int)(myByte >> (byteIndex-1))&1;
	byteIndex-=1;
	return result;
}

inline void clear_Bits(){
	byteIndex = 0 ;
}

inline int readInt(){
	int result = 0;
	result = result | (int)readByte();
	for(int i=0;i<3;i++){
		result = result<<8;
		result = result | (int)readByte();
	}
	return result;
}

int returnSign(int number){
	if(number == 0){
		return 0;
	}
	else if (number > 0){
		return 1;
	}
	else{
		return -1;
	}
}

inline int YCbCrtoRGB(double y, double cb,double cr){
	cb = cb - 128;
	cr = cr - 128;
	double rbg[]={ y+1.402*cr,y+1.772*cb,y-0.34414*cb-0.71414*cr};
	for(int i=0;i<3;i++){
		rbg[i] = round(rbg[i]);
		//rbg[i]+=128;
		rbg[i] = rbg[i] > 255 ? 255:rbg[i];
		rbg[i] = rbg[i] < 0 ? 0:rbg[i];
	}
	return (((int)rbg[0])<<16)|(((int)rbg[2])<<8)|(int)rbg[1];
}

void buildAllVLC(){
	initializeNode(macroblockAddressIncRoot);
	buildHuffman(macroblockAddressIncRoot,macroblockAddressIncrementTable,sizeof(macroblockAddressIncrementTable)/sizeof(macroblockAddressIncrementTable[0]));
	// test code
	//node *tmp; 
	//tmp = macroblockAddressIncRoot;
	//travelTree(tmp,0,0);
	// build the vlc table for the coding table macroblock type
	for(int i=0;i<4;i++){
		initializeNode(&macroblockTypeRoot[i]);
		buildHuffman(&macroblockTypeRoot[i],macroblocktypeTable[i],macroblocktypeTableSize[i]);
	}
	// build the motion vector backward forward code at vertical horizontal direction
	initializeNode(motionVectorRoot);
	buildHuffman(motionVectorRoot,motionVectorTable,sizeof(motionVectorTable)/sizeof(motionVectorTable[0]));
	//build the macroBlock pattern;
	initializeNode(macroblockPatternRoot);
	buildHuffman(macroblockPatternRoot,macroblockPatternTable,sizeof(macroblockPatternTable)/sizeof(macroblockPatternTable[0]));
	
	initializeNode(blockSizeLuminanceRoot);
	buildHuffman(blockSizeLuminanceRoot,block_size_luminance,sizeof(block_size_luminance)/sizeof(block_size_luminance[0]));
	
	initializeNode(blockSizeChrominanceRoot);
	buildHuffman(blockSizeChrominanceRoot,block_size_chrominance,sizeof(block_size_chrominance)/sizeof(block_size_chrominance[0]));
	
	initializeNode(dcCoefRoot);
	buildHuffman(dcCoefRoot,dc_coef_table,sizeof(dc_coef_table)/sizeof(dc_coef_table[0]));
	
	/*node *tmp = dcCoefRoot;
	toDCNext();
	travelDCTree(dcCoefRoot,0,0);*/
	//testVLC(blockSizeChrominanceRoot);
	//testVLC(macroblockPatternRoot);
	//testVLC(motionVectorRoot);
}

void testVLC(node* root){
	node *tmp = root;
	travelTree(tmp,0,0);
}

inline void toDCFirst(){
	node *tmp = dcCoefRoot;
	tmp = tmp->right;
	tmp->isLeaf = true;
}

inline void toDCNext(){
	node *tmp = dcCoefRoot;
	tmp = tmp->right;
	tmp->isLeaf = false;
}

inline int readnBits(int count){
	int result=0;
	result = result | readBits();
	for(int i=0;i<count-1;i++){
		result = result <<1;
		result = result | readBits();
	}
	return result;
}

inline int readnBytes(int count){ // need to use this really careful, a misuse will crash the whole program
	int result = 0;
	result = result | (int)readByte();
	for(int i=0;i<count-1;i++){
		result = result<<8;
		result = result | (int)readByte();
	}
	return result;
}

inline int peaknBits(int count){
	int tmpcount = count;
	uint8 tmpMyByte = myByte;
	int tmpbyteIndex = byteIndex;
	int tmpInputIndex = inputIndex;
	int result=0;
	fpos_t pos;
	fgetpos(my_fd,&pos);
	fsetpos(tmpfd, &pos);
	while(1){
		if(tmpbyteIndex == 0){
			if(tmpInputIndex >= currentBufferSize){
				size_t tmp;
				tmp = fread(&tmpMyByte,1,1,tmpfd);
			}
			else{
				tmpMyByte = buffer[tmpInputIndex];
				tmpInputIndex = tmpInputIndex +1;
			}
			tmpbyteIndex = 8;
		}
		result = result | (int)(tmpMyByte >> (tmpbyteIndex-1))&1;
		tmpbyteIndex -= 1;
		tmpcount -= 1;
		if(tmpcount!=0){
			result = result << 1;
		}
		else{
			break;
		}
	}
	return result;
}

inline void dividePel(picture* myPicture, int row,int col,float number){
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			myPicture->pelmap[row + i][col + j].y=(int)(round((float)(myPicture->pelmap[row + i][col + j].y)/number));
			myPicture->pelmap[row + i][col + j].cb=(int)(round((float)(myPicture->pelmap[row + i][col + j].cb)/number));
			myPicture->pelmap[row + i][col + j].cr=(int)(round((float)(myPicture->pelmap[row + i][col + j].cr)/number));
		}
	}
}

inline void simpleDividePel(pel** myPel, float number){
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			myPel[i][j].y = (int)(round((float)(myPel[i][j].y)/number));
			myPel[i][j].cb = (int)(round((float)(myPel[i][j].cb)/number));
			myPel[i][j].cr = (int)(round((float)(myPel[i][j].cr)/number));
		}
	}
}

inline void initializePel(picture* myPicture,int row,int col){
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			myPicture->pelmap[row + i][col + j].y=0;
			myPicture->pelmap[row + i][col + j].cb=0;
			myPicture->pelmap[row + i][col + j].cr=0;
		}
	}
}

inline void simpleInitializePel(pel** myPel){
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			myPel[i][j].y = 0;
			myPel[i][j].cb = 0;
			myPel[i][j].cr = 0;
		}
	}
}

inline void copyPel(picture* myPicture,picture* refPicture,int myRow,int myCol,int refRow,int refCol){
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			myPicture->pelmap[i+myRow][j+myCol].y += refPicture->pelmap[i + refRow][j + refCol].y;
			myPicture->pelmap[i+myRow][j+myCol].cb += refPicture->pelmap[i + refRow][j + refCol].cb;
			myPicture->pelmap[i+myRow][j+myCol].cr += refPicture->pelmap[i + refRow][j + refCol].cr;
		}
	}
}

inline void simpleCopyPel(pel** myPel,picture* refPicture,int refRow,int refCol){
	for(int i=0;i<16;i++){
		for(int j=0;j<16;j++){
			myPel[i][j].y += refPicture->pelmap[i + refRow][j + refCol].y;
			myPel[i][j].cb += refPicture->pelmap[i + refRow][j + refCol].cb;
			myPel[i][j].cr += refPicture->pelmap[i + refRow][j + refCol].cr;
		}
	}
}

inline int peaknBytes(int count){
	int tmpcount = count;
	int tmpInputIndex = inputIndex;
	int result=0;
	uint8 tmp_input;
	fpos_t pos;
	fgetpos(my_fd,&pos);
	fsetpos(tmpfd, &pos);
	while(1){
		if(tmpInputIndex >= currentBufferSize){
			size_t tmp;
			tmp = fread(&tmp_input,1,1,tmpfd);
		}
		else{
			tmp_input = buffer[tmpInputIndex];
			tmpInputIndex = tmpInputIndex + 1;
		}
		result = result | (int)(tmp_input);
		tmpcount -=1;
		if(tmpcount != 0){
			result = result << 8;
		}
		else{
			break;
		}
	}
	return result;
}

inline void readSkip(int count){ // need to use this function cautiously
	inputIndex += count;
	if(inputIndex >= currentBufferSize){
		readFile();
		inputIndex = 0;
	}
}

static void idct1(int *x, int y[8][8],int yIndex, int ps, int half) // 1D-IDCT
{
    int p, n;
    x[0] <<= 9, x[1] <<= 7, x[3] *= 181, x[4] <<= 9, x[5] *= 181, x[7] <<= 7;
    xmul(x[6], x[2], 277, 669, 0);
    xadd3(x[0], x[4], x[6], x[2], half);
    xadd3(x[1], x[7], x[3], x[5], 0);
    xmul(x[5], x[3], 251, 50, 6);
    xmul(x[1], x[7], 213, 142, 6);
    y[0][yIndex] = (x[0] + x[1]) >> ps;
    y[1][yIndex] = (x[4] + x[5]) >> ps;
    y[2][yIndex] = (x[2] + x[3]) >> ps;
    y[3][yIndex] = (x[6] + x[7]) >> ps;
    y[4][yIndex] = (x[6] - x[7]) >> ps;
    y[5][yIndex] = (x[2] - x[3]) >> ps;
    y[6][yIndex] = (x[4] - x[5]) >> ps;
    y[7][yIndex] = (x[0] - x[1]) >> ps;
}

void idct(int table[8][8]) // 2D 8x8 IDCT
{
    int i,table2[8][8];
	for (i = 0; i<8; i++){
		idct1(table[i],table2,i, 9, 1 << 8); // row
	}
	for (i = 0; i<8; i++){
		idct1(table2[i],table,i,12, 1 << 11); // col
	}
    //for (i = 0; i<8; i++) idct1(b + i * 8, b2 + i, 9, 1 << 8); // row
    //for (i = 0; i<8; i++) idct1(b2 + i * 8, b + i, 12, 1 << 11); // col
}

void initFile(){
	my_fd = fopen(fileName,"rb");
	tmpfd = fopen(fileName,"rb");
	if (my_fd==NULL || tmpfd == NULL){
		fputs ("File error",stderr);
		exit (1);
	}
	
	buffer = (uint8*)malloc(sizeof(uint8)*bufferSize);
	if(buffer == NULL){
		fputs("buffer init Error",stderr);
		exit(2);
	}
	
	fseek(my_fd, 0, SEEK_END);
	fileSize = ftell(my_fd); // get the file size
	rewind(my_fd);
	currentBufferSize=0;
	inputIndex=0;
	byteIndex = 0;
}

inline void readFile(){
	currentBufferSize = (long)fread(buffer, 1, bufferSize, my_fd);
	if(currentBufferSize <=0){
		fputs("readFile Something Wrong(maybe reach EOF)",stderr);
		exit(3);
	}
}

void readSequenceHeader(){
	readSkip(4);
	myVideo.horizontalSize = readnBits(12);// wide of the screen
	myVideo.verticalSize = readnBits(12); // height of the screen
	myVideo.mb_col = (myVideo.horizontalSize+15)/16;
	myVideo.mb_row = (myVideo.verticalSize + 15)/16;	
	myVideo.pel_aspect_ratio = pel_aspect_ratio_table[(int)readnBits(4)];
	myVideo.picture_rate = picture_rate_table[(int)readnBits(4)];
	myVideo.bit_rate = readnBits(18);
	myVideo.markerBit = readnBits(1);
	myVideo.vbvBufferSize = readnBits(10);
	myVideo.constrained_parameters_flag = readnBits(1);
	myVideo.load_intra_quantizer = readnBits(1);
	if(myVideo.load_intra_quantizer == 1){
		for(int i=0;i<8;i++){
			for(int j=0;j<8;j++){
				myVideo.intra_quantizer[i][j] = readByte();
			}
		}
	}
	else{
		for(int i=0;i<8;i++){
			for(int j=0;j<8;j++){
				myVideo.intra_quantizer[i][j] = default_intra_quantizer[i][j];
			}
		}
	}
	myVideo.load_non_intra_quantizer = readnBits(1);
	if(myVideo.load_non_intra_quantizer == 1){
		for(int i=0;i<8;i++){
			for(int j=0;j<8;j++){
				myVideo.non_intra_quantizer[i][j] = readByte();
			}
		}
	}
	else{
		for(int i=0;i<8;i++){
			for(int j=0;j<8;j++){
				myVideo.non_intra_quantizer[i][j] = default_non_intra_quantizer[i][j];
			}
		}
	}
	findHeader();
	int header=peaknBytes(4);// = findHeader();
	if(header == extension_start_code){
		readSkip(4);
		while(peaknBits(24) != 1){
			myVideo.extension_data.push_back((uint8)readnBits(8));
		}
		findHeader();
	}
	header=peaknBytes(4);
	if(header == user_start_code){
		readSkip(4);
		while(peaknBits(24) != 1){
			myVideo.user_data.push_back((uint8)readnBits(8));
		}
		findHeader();
	}
}

void decode_gop(gop* myGOP){
	// we already read the group start code in the beginning
	//cout<<"Decode GOP"<<endl;
	readSkip(4);
	
	//timeCode stuff 0w0/
	myGOP->drop_frame_flag = (uint8)readnBits(1);
	myGOP->time_code_hours = (uint8)readnBits(5);
	myGOP->time_code_mins = (uint8)readnBits(6);
	myGOP->markerBit = (uint8)readnBits(1);
	myGOP->time_code_secs = (uint8)readnBits(6);
	myGOP->time_code_pics = (uint8)readnBits(6); // finish reading the time code, I have no idea why it's all 0 except the fucking marker Bit 0w0
	// end of timeCode stuff 0w0/
	myGOP->closed_gop = (uint8)readnBits(1); // need to make sure the whole fucking stuff O  WO of this closed GOP stuff;3
	myGOP->broken_link = (uint8)readnBits(1); // need to make sure the whole fucking stuff O  WO  of this Gop broken link stuff
	
	findHeader();
	if(peaknBytes(4) == extension_start_code){
		readSkip(4);
		while(peaknBits(24)!=1){
			myGOP->extension_data.push_back((uint8)readnBits(8));
		}
		findHeader();
	}
	if(peaknBytes(4) == user_start_code){
		readSkip(4);
		while(peaknBits(24)!=1){
			myGOP->user_data.push_back((uint8)readnBits(8));
		}
		findHeader();
	}
	while(peaknBytes(4) == picture_start_code){
		picture myPicture;
		myPicture.bitmap = (int**) malloc(sizeof(int*)*(myVideo.mb_row<<4));
		myPicture.pelmap = (pel**) malloc(sizeof(pel*)*(myVideo.mb_row<<4));
		for(int j=0;j<(myVideo.mb_row<<4);j++){
			myPicture.bitmap[j] = (int*)malloc(sizeof(int)*(myVideo.mb_col<<4));
			myPicture.pelmap[j] = (pel*)malloc(sizeof(pel)*(myVideo.mb_col<<4));
		}
		decode_picture(&myPicture);
		//insert the picture into the map first 0.0/ sounds more easy
		//myGOP->pictures.push_back(myPicture);
		//myGOP->pictures.insert(pair<int,picture>(myPicture.temporal_reference,myPicture));
		//ensure the picture is ready to displya;
		decodePictureRGB(&myPicture);
		//decide the forward frame and the backward frame part;
		if(myPicture.picture_coding_type == I_type_picture || myPicture.picture_coding_type == P_type_picture){
			if(myVideo.backward_frame.size()!=0){
				myVideo.forward_frame.clear();
				myVideo.forward_frame.push_back(myVideo.backward_frame.back());
				myVideo.backward_frame.pop_back();
			}
			myVideo.backward_frame.push_back(myPicture);
		}
		else if (myPicture.picture_coding_type == B_type_picture){
			outputPicture(myPicture);
		}
	}
}

void decode_picture(picture* myPicture){
	readSkip(4);
	myPicture->temporal_reference = readnBits(10);
	//cout<<"Picture temporal Reference "<<myPicture->temporal_reference<<endl;
	myPicture->picture_coding_type = (uint8)readnBits(3);
	myPicture->vbv_delay = readnBits(16);
	myPicture->previousBlockAddress = -1;
	if(myPicture->picture_coding_type == P_type_picture || myPicture->picture_coding_type == B_type_picture){ // the 3 buffering to choose the frame
		myPicture->full_pel_forward_vector = (uint8)readnBits(1);
		//cout<<"full_pel_forward_vector "<<(int)myPicture->full_pel_forward_vector<<endl;
		myPicture->forward_f_code = (uint8)readnBits(3);
		//cout<<"forward_f_code "<<(int)myPicture->forward_f_code<<endl;
		myPicture->forward_r_size = (int)myPicture->forward_f_code - 1;
		myPicture->forward_f = 1 << ((int)myPicture->forward_r_size);
	}
	if (myPicture->picture_coding_type == B_type_picture){
		myPicture->full_pel_backward_vector = (uint8)readnBits(1);
		myPicture->backward_f_code = (uint8)readnBits(3);
		myPicture->backward_r_size = (int)myPicture->backward_f_code - 1;
		myPicture->backward_f = 1 << ((int)myPicture->backward_r_size);
	}
	int extraBit;
	while(peaknBits(1) == 1){
		myPicture->extra_bit = (uint8)readnBits(1);
		myPicture->extra_information.push_back((uint8)readnBits(8));
	}
	extraBit = readnBits(1);
	findHeader();
	
	if(peaknBytes(4) == extension_start_code){
		readSkip(4);
		while(peaknBits(24) != 1){
			myPicture->extension_data.push_back((uint8)readnBits(8));
		}
		findHeader();
	}
	if(peaknBytes(4) == user_start_code){
		readSkip(4);
		while(peaknBits(24) != 1){
			myPicture->user_data.push_back((uint8)readnBits(8));
		}
		findHeader();
	}
	// choose the forward backward frame for the picture
	if(myPicture->picture_coding_type == I_type_picture || myPicture->picture_coding_type == P_type_picture){
		if(myVideo.backward_frame.size()!=0){
			myVideo.forward_frame.clear();
			myVideo.forward_frame.push_back(myVideo.backward_frame.back());
			myVideo.backward_frame.pop_back();
			outputPicture(myVideo.forward_frame.back());
		}
	}
	// decode the slice now
	int header = peaknBytes(4);
	while(header >= slice_start_codes_min && header <= slice_start_codes_min){ //got a slie
		slice mySlice;
		decode_slice(&mySlice, myPicture);
		myPicture->slices.push_back(mySlice);
		header = peaknBytes(4);
	}
}

/*void outputGOP(gop* myGOP){
	//get the picture first
	for(int i=0;i < myGOP->pictures.size();i++){
		picture tmpPicture = myGOP->pictures[i];
		if(tmpPicture.picture_coding_type == I_type_picture){
			output_I_picture(tmpPicture);
		}
	}
}*/

void decodePictureRGB(picture *myPicture){
	//row 15 col 20
	//int **bitmap; //rgb map for bmp
	//cout<<"Going to output Picture "<<pictureIndex<<endl;
	for(auto sliceIt = myPicture->slices.begin();sliceIt != myPicture->slices.end();sliceIt ++ ){
		slice mySlice = *sliceIt;
		int dct_dc_y_past;
		int dct_dc_cr_past;
		int dct_dc_cb_past;
		int past_intra_address = -2;
		for(auto macroIt = mySlice.macroBlocks.begin();macroIt != mySlice.macroBlocks.end();macroIt ++ ){
			macroblock myMacroBlock = *macroIt;
			//cout<<(int)myMacroBlock.quant_scale<<endl;
			int mcuRow = (myMacroBlock.address/myVideo.mb_col);
			int mcuCol = (myMacroBlock.address%myVideo.mb_col);
			mcu *tmpMCU = &(myMacroBlock.myMCU);
			
			//check if the macroBlock is Intra or not
			if(myMacroBlock.macroblock_intra != 0){
				// dequantize part is following the example at video.doc page 46 section 2.4.4.1
				// the first y
				for(int i=1;i<64;i++){
					int row = i >> 3;
					int col = i & 7;
					tmpMCU->y[0][row][col] = (2 *tmpMCU->y[0][row][col] * myMacroBlock.quant_scale * myVideo.intra_quantizer[row][col])/16;
					if(!tmpMCU->y[0][row][col] & 1){
						tmpMCU->y[0][row][col] = tmpMCU->y[0][row][col] - returnSign(tmpMCU->y[0][row][col]);
					}
					if(tmpMCU->y[0][row][col] > 2047){
						tmpMCU->y[0][row][col] = 2047;
					}
					if(tmpMCU->y[0][row][col] < -2048){
						tmpMCU->y[0][row][col] = -2048;
					}
				}
				tmpMCU->y[0][0][0] = (tmpMCU->y[0][0][0] << 3);
				if(myMacroBlock.address - past_intra_address > 1){
					tmpMCU->y[0][0][0] = 1024 +tmpMCU->y[0][0][0];
				}
				else{
					tmpMCU->y[0][0][0] = dct_dc_y_past + tmpMCU->y[0][0][0];
				}
				dct_dc_y_past = tmpMCU->y[0][0][0];
				// the rest of the Y to decode
				for(int index =1;index<4;index ++){
					for(int i=1;i<64;i++){
						int row = i >>3;
						int col = i&7;
						tmpMCU->y[index][row][col] = (2 *tmpMCU->y[index][row][col] * myMacroBlock.quant_scale * myVideo.intra_quantizer[row][col])/16;
						if(!tmpMCU->y[index][row][col] & 1){
							tmpMCU->y[index][row][col] = tmpMCU->y[index][row][col] - returnSign(tmpMCU->y[index][row][col]);
						}
						if(tmpMCU->y[index][row][col] > 2047){
							tmpMCU->y[index][row][col] = 2047;
						}
						if(tmpMCU->y[index][row][col] < -2048){
							tmpMCU->y[index][row][col] = -2048;
						}
					}
					tmpMCU->y[index][0][0] = (tmpMCU->y[index][0][0] << 3) + dct_dc_y_past;
					dct_dc_y_past = tmpMCU->y[index][0][0];
				}
				
				// try to decode the cb cr part
				//decode cb first
				for(int i=1;i<64;i++){
					int row = i >>3;
					int col = i&7;
					tmpMCU->cb[row][col] = (2 *tmpMCU->cb[row][col] * myMacroBlock.quant_scale * myVideo.intra_quantizer[row][col])/16;
					if(!tmpMCU->cb[row][col] & 1){
						tmpMCU->cb[row][col] = tmpMCU->cb[row][col] - returnSign(tmpMCU->cb[row][col]);
					}
					if(tmpMCU->cb[row][col] > 2047){
						tmpMCU->cb[row][col] = 2047;
					}
					if(tmpMCU->cb[row][col] < -2048){
						tmpMCU->cb[row][col] = -2048;
					}
				}
				tmpMCU->cb[0][0] = (tmpMCU->cb[0][0] << 3);
				if(myMacroBlock.address - past_intra_address > 1){
					tmpMCU->cb[0][0] = 1024 +tmpMCU->cb[0][0];
				}
				else{
					tmpMCU->cb[0][0] = dct_dc_cb_past + tmpMCU->cb[0][0];
				}
				dct_dc_cb_past = tmpMCU->cb[0][0];
				
				//decode the cr
				for(int i=1;i<64;i++){
					int row = i >>3;
					int col = i&7;
					tmpMCU->cr[row][col] = (2 *tmpMCU->cr[row][col] * myMacroBlock.quant_scale * myVideo.intra_quantizer[row][col])/16;
					if(!tmpMCU->cr[row][col] & 1){
						tmpMCU->cr[row][col] = tmpMCU->cr[row][col] - returnSign(tmpMCU->cr[row][col]);
					}
					if(tmpMCU->cr[row][col] > 2047){
						tmpMCU->cr[row][col] = 2047;
					}
					if(tmpMCU->cr[row][col] < -2048){
						tmpMCU->cr[row][col] = -2048;
					}
				}
				tmpMCU->cr[0][0] = (tmpMCU->cr[0][0] << 3);
				if(myMacroBlock.address - past_intra_address > 1){
					tmpMCU->cr[0][0] = 1024 +tmpMCU->cr[0][0];
				}
				else{
					tmpMCU->cr[0][0] = dct_dc_cr_past + tmpMCU->cr[0][0];
				}
				dct_dc_cr_past = tmpMCU->cr[0][0];
				
				past_intra_address = myMacroBlock.address;
				// dequantize OK let's do the rest of the part
				//IDCT part
				for(int k=0;k<4;k++){
					idct(tmpMCU->y[k]);
				}
				idct(tmpMCU->cb);
				idct(tmpMCU->cr);
				//cout<<mcuRow<<" "<<mcuCol<<endl;
			}
			else{
				//y
				for(int i=0;i<4;i++){
					if((myMacroBlock.pattern_code)&(1<<(5-i))){
						for(int m=0;m<8;m++){
							for(int n=0;n<8;n++){
								if(tmpMCU->y[i][m][n]!=0){
									tmpMCU->y[i][m][n] = ((2 *tmpMCU->y[i][m][n] + returnSign(tmpMCU->y[i][m][n])) * myMacroBlock.quant_scale * myVideo.non_intra_quantizer[m][n])/16;
									if(!tmpMCU->y[i][m][n] & 1){
										tmpMCU->y[i][m][n] = tmpMCU->y[i][m][n] - returnSign(tmpMCU->y[i][m][n]);
									}
									if(tmpMCU->y[i][m][n] > 2047){
										tmpMCU->y[i][m][n] = 2047;
									}
									if(tmpMCU->y[i][m][n] < -2048){
										tmpMCU->y[i][m][n] = -2048;
									}
								}
							}
						}
						idct(tmpMCU->y[i]);
					}
				}
				//cb
				if((myMacroBlock.pattern_code)&(2)){
					for(int m=0;m<8;m++){
							for(int n=0;n<8;n++){
								if(tmpMCU->cb[m][n]!=0){
									tmpMCU->cb[m][n] = ((2 *tmpMCU->cb[m][n] + returnSign(tmpMCU->cb[m][n])) * myMacroBlock.quant_scale * myVideo.non_intra_quantizer[m][n])/16;
									if(!tmpMCU->cb[m][n] & 1){
										tmpMCU->cb[m][n] = tmpMCU->cb[m][n] - returnSign(tmpMCU->cb[m][n]);
									}
									if(tmpMCU->cb[m][n] > 2047){
										tmpMCU->cb[m][n] = 2047;
									}
									if(tmpMCU->cb[m][n] < -2048){
										tmpMCU->cb[m][n] = -2048;
									}
								}
							}
						}
						idct(tmpMCU->cb);
				}
				//cr
				if((myMacroBlock.pattern_code)&1){
					for(int m=0;m<8;m++){
							for(int n=0;n<8;n++){
								if(tmpMCU->cr[m][n]!=0){
									tmpMCU->cr[m][n] = ((2 *tmpMCU->cr[m][n] + returnSign(tmpMCU->cr[m][n])) * myMacroBlock.quant_scale * myVideo.non_intra_quantizer[m][n])/16;
									if(!tmpMCU->cr[m][n] & 1){
										tmpMCU->cr[m][n] = tmpMCU->cr[m][n] - returnSign(tmpMCU->cr[m][n]);
									}
									if(tmpMCU->cr[m][n] > 2047){
										tmpMCU->cr[m][n] = 2047;
									}
									if(tmpMCU->cr[m][n] < -2048){
										tmpMCU->cr[m][n] = -2048;
									}
								}
							}
						}
						idct(tmpMCU->cr);
				}
			}
			// change the stuff to picture
			for(int i=0;i<16;i++){
				for(int j=0;j<16;j++){
					int y_number=0;
					if(i >=8 && j>=8){
						y_number = 3;
					}
					else if (i>=8){
						y_number = 2;
					}
					else if (j>=8){
						y_number = 1;
					}
					int y = tmpMCU->y[y_number][i&7][j&7];
					int cb = tmpMCU->cb[i>>1][j>>1];
					int cr = tmpMCU->cr[i>>1][j>>1];
					if(myMacroBlock.macroblock_intra == 0 ){
						y = y + myPicture->pelmap[mcuRow*16 + i][mcuCol*16+j].y;
						cb = cb + myPicture->pelmap[mcuRow*16 + i][mcuCol*16+j].cb;
						cr = cr + myPicture->pelmap[mcuRow*16 + i][mcuCol*16+j].cr;
					}
					int color = YCbCrtoRGB((double)y,(double)cb,(double)cr);
					/*if(pictureIndex <= 2 && myMacroBlock.address == 51){
						cout<<pictureIndex<<"    "<<myMacroBlock.address<<"  "<<y<<" "<<cb<<" "<<cr<<"  "<<((color>>16)&255)<<" "<<((color>>8)&255)<<" "<<((color)&255)<<endl;
					}*/
					myPicture->bitmap[mcuRow*16+i][mcuCol*16+j]=color;
					myPicture->pelmap[mcuRow*16 + i][mcuCol*16+j].y = y;
					myPicture->pelmap[mcuRow*16 + i][mcuCol*16+j].cb = cb;
					myPicture->pelmap[mcuRow*16 + i][mcuCol*16+j].cr = cr;
				}
			}
			//end of macroBlock loop
		}
	    //end of slice loop
	}
	/*if(pictureIndex == 1){
		for(int i=0;i<myVideo.mb_row*16;i++){
			for(int j=0;j<myVideo.mb_col*16;j++){
				//cout<<((myPicture->bitmap[i][j]>>16)&255)<<"  "<<((myPicture->bitmap[i][j]>>8)&255)<<"   "<<((myPicture->bitmap[i][j])&255)<<endl;
				myPicture->bitmap[i][j] = 0;
			}
			//cout<<endl;
		}
	}*/
}

inline void outputPicture(picture myPicture){
	/*char pictureName[64];
	sprintf(pictureName,"%d.bmp",pictureIndex);
	string title(pictureName);*/
	//pictureIndex += 1;
	frame newFrame;
	newFrame.bitmap = (pixel*) malloc(sizeof(pixel)*(myVideo.mb_row<<4)*(myVideo.mb_col<<4));
	/*
		for ( int y=h-1; y>=0; y-- )
	{
		for ( int x=0; x<w; x++ )
		{
			int b=bitmap[y][x].b;
			int g=bitmap[y][x].g;
			int r=bitmap[y][x].r;
			unsigned char pixel[3];
			pixel[0] = b&255;
			pixel[1] = g&255;
			pixel[2] = r&255;

			outfile.write( (char*)pixel, 3 );
		}
		outfile.write( (char*)pad, padSize );
	}
	*/
	int tmpindex=0;
	int bitMapCol = myVideo.mb_col<<4;
	for(int y = (myVideo.mb_row<<4)-1;y>=0;y--){
		for(int x=0;x<bitMapCol;x++){
			newFrame.bitmap[tmpindex].r = (uint8)((myPicture.bitmap[y][x]>>16)&255);
			newFrame.bitmap[tmpindex].g = (uint8)((myPicture.bitmap[y][x]>>8)&255);
			newFrame.bitmap[tmpindex].b = (uint8)((myPicture.bitmap[y][x])&255);
			tmpindex = tmpindex + 1;
		}
	}
	readytoDraw = true;
	AllBuffer.push_back(newFrame);
	/*
		int b=mypixel&255;
		int g=(mypixel>>8)&255;
		int r=(mypixel>>16)&255;
	*/
	/*-
	/*if(pictureIndex >= 10){
		exit(0);
	}*/
	//output_bmp(title,myPicture.bitmap);
}

inline void initializeNode(node* cur){
	cur->left = NULL;
	cur->right = NULL;
	cur -> isLeaf = false;
}

void travelTree(node* cur,int val,int index){ // debug code
	if(cur->isLeaf){
		//cout<<"Found a leaf~ Code "<<val<<" "<<cur->value<<" Length"<<index<<endl;
		cout<<"Len "<<index<<" Code "<<val<<" Value "<<cur->value<<endl;
	}
	else{
		if(cur->left !=NULL){
			int newLeft;
			newLeft = val*2;
			travelTree(cur->left,newLeft,index+1);
		}
		if(cur->right!=NULL){
			int newRight;
			newRight = val*2;
			newRight++;
			travelTree(cur->right,newRight,index+1);
		}
	}
}

void travelDCTree(node* cur,int val,int index){ // debug code
	if(cur->isLeaf){
		//cout<<"Found a leaf~ Code "<<val<<" "<<cur->value<<" Length"<<index<<endl;
		if(cur->value == -2){
			cout<<"Len "<<index<<" Code "<<val<<" Escape"<<endl;
		}
		else if(cur->value == -1){
			cout<<"Len "<<index<<" Code "<<val<<" EOB"<<endl;
		}
		else{
			cout<<"Len "<<index<<" Code "<<val<<" Run "<<((cur->value)>>6)<<"  level  "<<((cur->value) & 63)<<"  "<<cur->value<<endl;
		}
	}
	else{
		if(cur->left !=NULL){
			int newLeft;
			newLeft = val*2;
			travelDCTree(cur->left,newLeft,index+1);
		}
		if(cur->right!=NULL){
			int newRight;
			newRight = val*2;
			newRight++;
			travelDCTree(cur->right,newRight,index+1);
		}
	}
}

void buildHuffman(node* _root,pair<const char*,int>* table,int count){
	node *root = _root;
	node *cur;
	int index;
	for(index=0;index<count;index++){
		cur = _root;
		for(int i=0;i<strlen(table[index].first);i++){
			if(table[index].first[i]=='1'){
				if(cur->right == NULL){
					node *tmpNode;
					tmpNode = (node*)malloc(sizeof(node));
					initializeNode(tmpNode);
					cur->right = tmpNode;
				}
				cur = cur->right;
			}
			else if(table[index].first[i] == '0'){
				if(cur->left==NULL){
					node *tmpNode;
					tmpNode = (node*)malloc(sizeof(node));
					initializeNode(tmpNode);
					cur->left = tmpNode;
				}
				cur = cur->left;
			}
		}
		cur->isLeaf = true;
		cur->value = table[index].second;
	}
}

inline int huffmanDecode(node *root){
	node *curpos = root;
	while(curpos->isLeaf == false){
		int bit = readBits();
		if(bit == 1 && curpos->right != NULL){
			curpos = curpos->right;
		}
		else if (bit == 0 && curpos->left != NULL){
			curpos = curpos->left;
		}
		else{
			fputs("wtf decode failed can not find the value for the huffmanCode",stderr);
			exit(5);
		}
	}
	return curpos->value;
}

void decode_slice(slice* mySlice, picture* myPicture){
	int header = readnBytes(4);
	mySlice->verticalPos = (header&255);
	//cout<<"Decode Slice "<<mySlice->verticalPos<<" "<<endl;
	mySlice->scale = (uint8)readnBits(5);
	mySlice->extra_bit = 0;
	int extraBit;
	while(peaknBits(1) == 1){
		mySlice->extra_bit = (uint8)readnBits(1);
		mySlice->extra_information.push_back((uint8)readnBits(8));
	}
	extraBit = (uint8)readnBits(1);
	
	// reset the motion Vector stuff
	myPicture->recon_down_for_prev = 0; //vertical should be row
	myPicture->recon_right_for_prev = 0;// horizontal should be col
	myPicture->recon_down_back_prev = 0;
	myPicture->recon_right_back_prev = 0;
	
	while(peaknBits(23)!=0){
		macroblock myMacroBlock;
		decode_macroblock(&myMacroBlock, mySlice, myPicture);
		mySlice->macroBlocks.push_back(myMacroBlock);
	}
	findHeader();
}

void decode_macroblock(macroblock* myMacroBlock,slice* mySlice, picture* myPicture){
	int tmp_inc;
	tmp_inc = 0;
	while(1){
		int result = huffmanDecode(macroblockAddressIncRoot);
		if(result == 34){
			;
		}
		else if (result == 35){
			tmp_inc = tmp_inc + 33;
		}
		else if (result <=33 && result >=1){
			tmp_inc += result;
			break;
		}
		else{
			fputs("wtf decode something else  not in the table",stderr);
			exit(5);
		}
	}
	// if there's skipped macroBlock, reconstruct the part to make sure that no macro block is missed;
	for(int i=1;i<tmp_inc;i++){
		if(myPicture->picture_coding_type == P_type_picture){
			myPicture->recon_down_for_prev = 0;
			myPicture->recon_right_for_prev = 0;
		}
		myMacroBlock->recon_down_for = myPicture->recon_down_for_prev;
		myMacroBlock->recon_right_for = myPicture->recon_right_for_prev;
		int skippedAddress = myPicture->previousBlockAddress + i;
		reconstructPictureFromSkippedMacroBlock(myPicture,skippedAddress,mySlice);
	}
	myMacroBlock->address = myPicture->previousBlockAddress + tmp_inc;
	//cout<<"Decode MacroBlock "<<myMacroBlock->address + 1<<endl;
	//cout<<"Previous Block Address inc "<<tmp_inc<<endl;
	myPicture->previousBlockAddress = myMacroBlock->address;
	// decode the type of macroblock
	int result = huffmanDecode(&macroblockTypeRoot[(int)(myPicture->picture_coding_type)-1]);
	//cout<<"MacroBlock type "<<result<<endl;
	myMacroBlock->macroblock_quant = (((result) >> 4 )& 1);
	myMacroBlock->macroblock_motion_forward = (((result) >> 3 )& 1);
	myMacroBlock->macroblock_motion_backward = (((result) >> 2 )& 1);
	myMacroBlock->macroblock_pattern = (((result) >> 1 )& 1);
	myMacroBlock->macroblock_intra = ((result) & 1);
	
	if(myMacroBlock->macroblock_quant == 1){
		myMacroBlock->quant_scale = (uint8)readnBits(5);
		mySlice->scale = myMacroBlock->quant_scale;
	}
	else{
		myMacroBlock->quant_scale = mySlice->scale;
	}
	
	if(myMacroBlock->macroblock_motion_forward == 1){
		myMacroBlock->motion_horizontal_forward_code = huffmanDecode(motionVectorRoot);//read the vlc stuff
		//cout<<"motion_horizontal_forward_code  "<<myMacroBlock->motion_horizontal_forward_code<<"  "<<myMacroBlock->motion_horizontal_forward_r<<endl;
		if(myPicture->forward_f != 1 && myMacroBlock->motion_horizontal_forward_code != 0){
			//cout<<"might at wrong here"<<endl;
			myMacroBlock->motion_horizontal_forward_r = readnBits((int)myPicture->forward_r_size);
		}
		else{
			myMacroBlock->motion_horizontal_forward_r = 0;
		}
		myMacroBlock->motion_vertical_forward_code = huffmanDecode(motionVectorRoot);
		//cout<<"motion_vertical_forward_code  "<<myMacroBlock->motion_vertical_forward_code<<"  "<<myMacroBlock->motion_vertical_forward_r<<endl;
		if(myPicture->forward_f != 1 && myMacroBlock->motion_vertical_forward_code != 0){
			//cout<<"might at wrong here"<<endl;
			myMacroBlock->motion_vertical_forward_r = readnBits((int)myPicture->forward_r_size);
		}
		else{
			myMacroBlock->motion_vertical_forward_r = 0;
		}
		//cout<<"motion_horizontal_forward_code  "<<myMacroBlock->motion_horizontal_forward_code<<"  "<<myMacroBlock->motion_horizontal_forward_r<<endl;
		//cout<<"motion_vertical_forward_code  "<<myMacroBlock->motion_vertical_forward_code<<"  "<<myMacroBlock->motion_vertical_forward_r<<endl;
	}
	else{
		if(myPicture->picture_coding_type == P_type_picture){//reset the predictor in P type frame
			myMacroBlock->recon_right_for = 0;
			myMacroBlock->recon_down_for = 0;
			myPicture->recon_right_for_prev = 0;
			myPicture->recon_down_for_prev = 0;
		}
		
	}
	
	if(myMacroBlock->macroblock_motion_backward == 1){
		myMacroBlock->motion_horizontal_backward_code = huffmanDecode(motionVectorRoot);//read the vlc stuff
		if(myPicture->backward_f != 1 && myMacroBlock->motion_horizontal_backward_code != 0){
			myMacroBlock->motion_horizontal_backward_r = readnBits((int)myPicture->backward_r_size);
		}
		myMacroBlock->motion_vertical_backward_code = huffmanDecode(motionVectorRoot);
		if(myPicture->backward_f != 1 && myMacroBlock->motion_vertical_backward_code != 0){
			myMacroBlock->motion_vertical_backward_r = readnBits((int)myPicture->backward_r_size);
		}
	}
	
	if(myMacroBlock->macroblock_pattern == 1){ //read pattern code
		myMacroBlock->pattern_code = huffmanDecode(macroblockPatternRoot);//read the vlc stuff
	}
	else if(myMacroBlock->macroblock_intra == 1){
		myMacroBlock->pattern_code = 63; //'111111'
	}
	else{
		myMacroBlock->pattern_code = 0;
	}
	
	if(myMacroBlock->macroblock_intra == 0){ // if it's not an intra coded macro, we need to do motion compensation
		calculateMotionVector(myPicture,myMacroBlock);
		doMotionCompensation(myPicture,myMacroBlock);
	}
	else{
		myMacroBlock->macroblock_motion_forward = 0; //reset the predictor
		myMacroBlock->macroblock_motion_backward = 0;
		myPicture->recon_down_for_prev = 0;
		myPicture->recon_right_for_prev = 0;
		myPicture->recon_down_back_prev = 0;
		myPicture->recon_right_back_prev = 0;
	}
	//read the block stuff
	memset(myMacroBlock->myMCU.y[0],0,sizeof(int)*64);
	memset(myMacroBlock->myMCU.y[1],0,sizeof(int)*64);
	memset(myMacroBlock->myMCU.y[2],0,sizeof(int)*64);
	memset(myMacroBlock->myMCU.y[3],0,sizeof(int)*64);
	memset(myMacroBlock->myMCU.cb,0,sizeof(int)*64);
	memset(myMacroBlock->myMCU.cr,0,sizeof(int)*64);
	for(int index=0;index<6;index++){
		if((myMacroBlock->pattern_code)&(1<<(5-index))){
			decode_block(myMacroBlock,index, myPicture);
		}
	}
	//check if the picture is D Frame
	if(myPicture->picture_coding_type == D_type_picture){
		int tmp_int = (int)readBits();
		if(tmp_int != 1){
			fputs("wtf failed",stderr);
			exit(7);
		}
	}
}

void calculateMotionVector(picture* myPicture,macroblock* myMacroBlock){
	//forward Vector
	// the part in spec 2.4.4.2
	//cout<<myPicture->recon_right_for_prev<<"           "<<myPicture->recon_down_for_prev<<"            ";
	int complement_horizontal_forward_r,complement_horizontal_backward_r,complement_vertical_forward_r,complement_vertical_backward_r,right_little,right_big,down_little,down_big,max,min;
	if(myMacroBlock->macroblock_motion_forward == 1){
		if(myPicture->forward_f == 1 || myMacroBlock-> motion_horizontal_forward_code == 0){
			complement_horizontal_forward_r = 0;
		}
		else{
			complement_horizontal_forward_r = myPicture->forward_f - 1 - myMacroBlock->motion_horizontal_forward_r;
		}
		if(myPicture->forward_f == 1 || myMacroBlock->motion_vertical_forward_code == 0){
			complement_vertical_forward_r = 0 ;
		}
		else{
			complement_vertical_forward_r = myPicture->forward_f  -1 - myMacroBlock->motion_vertical_forward_r;
		}
		
		right_little = myMacroBlock->motion_horizontal_forward_code * myPicture->forward_f;
		if(right_little == 0){
			right_big = 0;
		}
		else{
			if(right_little > 0){
				right_little = right_little - complement_horizontal_forward_r;
				right_big = right_little - 32 * myPicture->forward_f;
			}
			else{
				right_little = right_little + complement_horizontal_forward_r;
				right_big = right_little + 32* myPicture->forward_f;
			}
		}
		
		down_little = myMacroBlock->motion_vertical_forward_code * myPicture->forward_f;
		if(down_little == 0){
			down_big = 0;
		}
		else{
			if(down_little > 0){
				down_little = down_little - complement_vertical_forward_r;
				down_big = down_little - 32* myPicture->forward_f;
			}
			else{
				down_little = down_little + complement_vertical_forward_r;
				down_big = down_little + 32* myPicture->forward_f;
			}
		}
		
		max = (16 * myPicture->forward_f ) - 1;
		min = (-16 * myPicture->forward_f);
		if(myPicture->recon_right_for_prev + right_little <= max && myPicture->recon_right_for_prev + right_little >= min){
			myMacroBlock->recon_right_for = myPicture->recon_right_for_prev + right_little;
		}
		else{
			myMacroBlock->recon_right_for = myPicture->recon_right_for_prev + right_big;
		}
		myPicture->recon_right_for_prev = myMacroBlock->recon_right_for;
		if(myPicture->full_pel_forward_vector == 1){
			myMacroBlock->recon_right_for = (myMacroBlock->recon_right_for << 1);
		}
		
		if((myPicture->recon_down_for_prev + down_little) <=max && (myPicture->recon_down_for_prev + down_little) >= min){
			myMacroBlock->recon_down_for = myPicture->recon_down_for_prev + down_little;
		}
		else{
			myMacroBlock->recon_down_for = myPicture->recon_down_for_prev + down_big;
		}
		myPicture->recon_down_for_prev = myMacroBlock->recon_down_for;
		//cout<<pictureIndex<<"  forward   "<<myMacroBlock->address + 1<<" "<<myMacroBlock->recon_right_for<<" "<<myMacroBlock->recon_down_for<<endl;
		/*if(pictureIndex == 19){
			cout<<myMacroBlock->address + 1<<" "<<myMacroBlock->recon_right_for<<" "<<myMacroBlock->recon_down_for<<endl;
		}*/
	}
	//backward_vector
	if(myMacroBlock->macroblock_motion_backward){
		if(myPicture->backward_f == 1 || myMacroBlock->motion_horizontal_backward_code==0){
			complement_horizontal_backward_r = 0;
		}
		else{
			complement_horizontal_backward_r = myPicture->backward_f -1 - myMacroBlock->motion_horizontal_backward_r;
		}
		
		if(myPicture->backward_f == 1 || myMacroBlock->motion_vertical_backward_code==0){
			complement_vertical_backward_r = 0;
		}
		else{
			complement_vertical_backward_r = myPicture->backward_f -1 - myMacroBlock->motion_vertical_backward_r;
		}
		
		right_little = myMacroBlock->motion_horizontal_backward_code * myPicture->backward_f;
		if(right_little == 0){
			right_big = 0;
		}
		else{
			if(right_little > 0){
				right_little = right_little - complement_horizontal_backward_r;
				right_big = right_little - 32 * myPicture->backward_f;
			}else{
				right_little = right_little + complement_horizontal_backward_r;
				right_big = right_little + 32 * myPicture->backward_f;
			}
		}
		
		down_little = myMacroBlock->motion_vertical_backward_code * myPicture->backward_f;
		if(down_little == 0){
			down_big = 0;
		}
		else{
			if(down_little > 0){
				down_little = down_little - complement_vertical_backward_r;
				down_big = down_little - 32 * myPicture->backward_f;
			}
			else{
				down_little = down_little + complement_vertical_backward_r;
				down_big = down_little + 32*myPicture->backward_f;
			}
		}
		
		//reconstruct
		max = (16 * myPicture->backward_f ) - 1;
		min = (-16 * myPicture->backward_f);
		if(myPicture->recon_right_back_prev + right_little <= max && myPicture->recon_right_back_prev + right_little >= min){
			myMacroBlock->recon_right_back = myPicture->recon_right_back_prev + right_little;
		}
		else{
			myMacroBlock->recon_right_back = myPicture->recon_right_back_prev + right_big;
		}
		myPicture->recon_right_back_prev = myMacroBlock->recon_right_back;
		if(myPicture->full_pel_forward_vector == 1){
			myMacroBlock->recon_right_back = (myMacroBlock->recon_right_back << 1);
		}
		
		if((myPicture->recon_down_back_prev + down_little) <=max && (myPicture->recon_down_back_prev + down_little) >= min){
			myMacroBlock->recon_down_back = myPicture->recon_down_back_prev + down_little;
		}
		else{
			myMacroBlock->recon_down_back = myPicture->recon_down_back_prev + down_big;
		}
		myPicture->recon_down_back_prev = myMacroBlock->recon_down_back;
		//cout<<myMacroBlock->address + 1<<"  back "<<myMacroBlock->recon_right_back<<" "<<myMacroBlock->recon_down_back<<endl;
	}
	
}

void reconstructPictureFromSkippedMacroBlock(picture* myPicture,int address, slice* mySlice){
	int bitMapCol = (address%myVideo.mb_col) << 4;
	int bitMapRow = (address/myVideo.mb_col) << 4;
	if(myPicture-> picture_coding_type == P_type_picture){ // just copy the whold blok to there
		for(int i=0;i<16;i++){ //copy the part 
			for(int j=0;j<16;j++){
				// copy the pel past stuff to the pel map for later use
				myPicture->bitmap[i + bitMapRow ][j + bitMapCol] = (myVideo.forward_frame.back()).bitmap[i + bitMapRow][j + bitMapCol];
				myPicture->pelmap[i + bitMapRow][j + bitMapCol].y = (myVideo.forward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].y;
				myPicture->pelmap[i + bitMapRow][j + bitMapCol].cb = (myVideo.forward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].cb;
				myPicture->pelmap[i + bitMapRow][j + bitMapCol].cr = (myVideo.forward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].cr;
			}
		}
	}
	else if (myPicture->picture_coding_type == B_type_picture){
		// needs to do the same stuff then the prior MacroBlock
		if(mySlice->macroBlocks.empty()){
			// something is wrong
			fputs("Something is wrong when copying blocks for B type picture",stderr);
			exit(11);
		}
		macroblock prior = mySlice->macroBlocks.back();
		pel **pel_past,**pel_future;
		pel_past = (pel**)malloc(sizeof(pel*)*16);
		pel_future = (pel**)malloc(sizeof(pel*)*16);
		for(int i=0;i<16;i++){
			pel_past[i] = (pel*)malloc(sizeof(pel)*16);
			pel_future[i] = (pel*)malloc(sizeof(pel)*16);
		}
		
		if(prior.macroblock_motion_forward==1 && prior.macroblock_motion_backward == 1){
			simpleInitializePel(pel_past);
			simpleInitializePel(pel_future);
			simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitMapRow,bitMapCol);
			simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitMapRow,bitMapCol);
			for(int i=0;i<16;i++){
				for(int j=0;j<16;j++){//copy the pel value
					int y,cb,cr;
					y=myPicture->pelmap[i + bitMapRow][j+bitMapCol].y = (int)(round(((float)(pel_past[i][j].y + pel_future[i][j].y)/2.0)));
					cb=myPicture->pelmap[i + bitMapRow][j+bitMapCol].cb = (int)(round(((float)(pel_past[i][j].cb + pel_future[i][j].cb)/2.0)));
					cr=myPicture->pelmap[i + bitMapRow][j+bitMapCol].cr = (int)(round(((float)(pel_past[i][j].cr + pel_future[i][j].cr)/2.0)));
					myPicture->bitmap[i + bitMapRow][j+bitMapCol] = YCbCrtoRGB((double)y,(double)cb,(double)cr);
				}
			}
		}
		else if(prior.macroblock_motion_forward == 1){
			for(int i=0;i<16;i++){ //copy the pel value
				for(int j=0;j<16;j++){
					// copy the pel past stuff to the pel map for later use
					myPicture->bitmap[i + bitMapRow ][j + bitMapCol] = (myVideo.forward_frame.back()).bitmap[i + bitMapRow][j + bitMapCol];
					myPicture->pelmap[i + bitMapRow][j + bitMapCol].y = (myVideo.forward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].y;
					myPicture->pelmap[i + bitMapRow][j + bitMapCol].cb = (myVideo.forward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].cb;
					myPicture->pelmap[i + bitMapRow][j + bitMapCol].cr = (myVideo.forward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].cr;
				}
			}
		}
		else if(prior.macroblock_motion_backward == 1){
			for(int i=0;i<16;i++){
				for(int j=0;j<16;j++){
					// copy the pel past stuff to the pel map for later use
					myPicture->bitmap[i + bitMapRow ][j + bitMapCol] = (myVideo.backward_frame.back()).bitmap[i + bitMapRow][j + bitMapCol];
					myPicture->pelmap[i + bitMapRow][j + bitMapCol].y = (myVideo.backward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].y;
					myPicture->pelmap[i + bitMapRow][j + bitMapCol].cb = (myVideo.backward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].cb;
					myPicture->pelmap[i + bitMapRow][j + bitMapCol].cr = (myVideo.backward_frame.back()).pelmap[i + bitMapRow][j + bitMapCol].cr;
				}
			}
		}
		
	}
}

void doMotionCompensation(picture* myPicture,macroblock* myMacroBlock){
	int bitmapCol,bitmapRow; // get where the part of macroblock is in the picture
	bitmapRow = (myMacroBlock->address / myVideo.mb_col)<<4;
	bitmapCol = (myMacroBlock->address % myVideo.mb_col )<< 4;
	if(myPicture->picture_coding_type == P_type_picture){
		int right_for,down_for,right_half_for,down_half_for;
		right_for = (myMacroBlock->recon_right_for >> 1);
		down_for = (myMacroBlock->recon_down_for >> 1);
		right_half_for = myMacroBlock->recon_right_for - 2*right_for;
		down_half_for = myMacroBlock->recon_down_for - 2*down_for;
		initializePel(myPicture,bitmapRow,bitmapCol);
		copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for,bitmapCol + right_for);
		if(right_half_for != 0 && down_half_for != 0){
			copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for);
			copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for ,bitmapCol + right_for +1);
			copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for+1);
			dividePel(myPicture,bitmapRow,bitmapCol,4);
		}
		else if (right_half_for !=0){
			copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for,bitmapCol + right_for +1);
			dividePel(myPicture,bitmapRow,bitmapCol,2);
		}
		else if (down_half_for !=0){
			copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for);
			dividePel(myPicture,bitmapRow,bitmapCol,2);
		}
		
	}
	else if (myPicture->picture_coding_type == B_type_picture){//motion compensation for B type
		pel **pel_past,**pel_future;
		pel_past = (pel**)malloc(sizeof(pel*)*16);
		pel_future = (pel**)malloc(sizeof(pel*)*16);
		for(int i=0;i<16;i++){
			pel_past[i] = (pel*)malloc(sizeof(pel)*16);
			pel_future[i] = (pel*)malloc(sizeof(pel)*16);
		}
		if(myMacroBlock->macroblock_motion_forward !=0 && myMacroBlock->macroblock_motion_backward !=0){ //calculate both for and back and get the average
			//motion forward
			int right_for,down_for,right_half_for,down_half_for;
			right_for = (myMacroBlock->recon_right_for >> 1);
			down_for = (myMacroBlock->recon_down_for >> 1);
			right_half_for = myMacroBlock->recon_right_for - 2*right_for;
			down_half_for = myMacroBlock->recon_down_for - 2*down_for;
			simpleInitializePel(pel_past);
			simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for,bitmapCol + right_for);
			if(right_half_for != 0 && down_half_for != 0){
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for);
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for+1,bitmapCol + right_for);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for ,bitmapCol + right_for +1);
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for,bitmapCol + right_for+1);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for+1);
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for+1,bitmapCol + right_for+1);
				//dividePel(myPicture,bitmapRow,bitmapCol,4);
				simpleDividePel(pel_past,4);
			}
			else if (right_half_for !=0){
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for,bitmapCol + right_for+1);
				simpleDividePel(pel_past,2);
			}
			else if (down_half_for !=0){
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for +1,bitmapCol + right_for);
				simpleDividePel(pel_past,2);
			}
			// motion backward
			int right_back,down_back,right_half_back,down_half_back;
			right_back = (myMacroBlock->recon_right_back >> 1);
			down_back = (myMacroBlock->recon_down_back >> 1);
			right_half_back = myMacroBlock->recon_right_back - 2*right_back;
			down_half_back = myMacroBlock->recon_down_back - 2*down_back;
			simpleInitializePel(pel_future);
			simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back,bitmapCol + right_back);
			if(right_half_back != 0 && down_half_back != 0){
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for);
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back+1,bitmapCol + right_back);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for ,bitmapCol + right_for +1);
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back,bitmapCol + right_back+1);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for+1);
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back+1,bitmapCol + right_back+1);
				//dividePel(myPicture,bitmapRow,bitmapCol,4);
				simpleDividePel(pel_future,4);
			}
			else if (right_half_back !=0){
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back,bitmapCol + right_back+1);
				simpleDividePel(pel_future,2);
			}
			else if (down_half_back !=0){
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back +1,bitmapCol + right_back);
				simpleDividePel(pel_future,2);
			}
			
			for(int i=0;i<16;i++){
				for(int j=0;j<16;j++){
					int y,cb,cr;
					y=myPicture->pelmap[i + bitmapRow][j+bitmapCol].y = (int)(round(((float)(pel_past[i][j].y + pel_future[i][j].y)/2.0)));
					cb=myPicture->pelmap[i + bitmapRow][j+bitmapCol].cb = (int)(round(((float)(pel_past[i][j].cb + pel_future[i][j].cb)/2.0)));
					cr=myPicture->pelmap[i + bitmapRow][j+bitmapCol].cr = (int)(round(((float)(pel_past[i][j].cr + pel_future[i][j].cr)/2.0)));
				}
			}
			
		}
		else if (myMacroBlock->macroblock_motion_forward != 0 ){ //calculate ForMV
			//motion forward
			int right_for,down_for,right_half_for,down_half_for;
			right_for = (myMacroBlock->recon_right_for >> 1);
			down_for = (myMacroBlock->recon_down_for >> 1);
			right_half_for = myMacroBlock->recon_right_for - 2*right_for;
			down_half_for = myMacroBlock->recon_down_for - 2*down_for;
			simpleInitializePel(pel_past);
			simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for,bitmapCol + right_for);
			if(right_half_for != 0 && down_half_for != 0){
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for);
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for+1,bitmapCol + right_for);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for ,bitmapCol + right_for +1);
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for,bitmapCol + right_for+1);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for+1);
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for+1,bitmapCol + right_for+1);
				//dividePel(myPicture,bitmapRow,bitmapCol,4);
				simpleDividePel(pel_past,4);
			}
			else if (right_half_for !=0){
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for,bitmapCol + right_for+1);
				simpleDividePel(pel_past,2);
			}
			else if (down_half_for !=0){
				simpleCopyPel(pel_past,&(myVideo.forward_frame.back()),bitmapRow + down_for +1,bitmapCol + right_for);
				simpleDividePel(pel_past,2);
			}
			
			for(int i=0;i<16;i++){
				for(int j=0;j<16;j++){
					int y,cb,cr;
					y=myPicture->pelmap[i + bitmapRow][j+bitmapCol].y = pel_past[i][j].y;
					cb=myPicture->pelmap[i + bitmapRow][j+bitmapCol].cb = pel_past[i][j].cb;
					cr=myPicture->pelmap[i + bitmapRow][j+bitmapCol].cr = pel_past[i][j].cr;
				}
			}	
		}
		else if (myMacroBlock->macroblock_motion_backward !=0){ //calculate backMV
			// motion backward
			int right_back,down_back,right_half_back,down_half_back;
			right_back = (myMacroBlock->recon_right_back >> 1);
			down_back = (myMacroBlock->recon_down_back >> 1);
			right_half_back = myMacroBlock->recon_right_back - 2*right_back;
			down_half_back = myMacroBlock->recon_down_back - 2*down_back;
			simpleInitializePel(pel_future);
			simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back,bitmapCol + right_back);
			if(right_half_back != 0 && down_half_back != 0){
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for);
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back+1,bitmapCol + right_back);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for ,bitmapCol + right_for +1);
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back,bitmapCol + right_back+1);
				//copyPel(myPicture,&(myVideo.forward_frame.back()),bitmapRow,bitmapCol,bitmapRow + down_for +1,bitmapCol + right_for+1);
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back+1,bitmapCol + right_back+1);
				//dividePel(myPicture,bitmapRow,bitmapCol,4);
				simpleDividePel(pel_future,4);
			}
			else if (right_half_back !=0){
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back,bitmapCol + right_back+1);
				simpleDividePel(pel_future,2);
			}
			else if (down_half_back !=0){
				simpleCopyPel(pel_future,&(myVideo.backward_frame.back()),bitmapRow + down_back +1,bitmapCol + right_back);
				simpleDividePel(pel_future,2);
			}
			
			for(int i=0;i<16;i++){//copy the pel value
				for(int j=0;j<16;j++){
					int y,cb,cr;
					y=myPicture->pelmap[i + bitmapRow][j+bitmapCol].y = pel_future[i][j].y;
					cb=myPicture->pelmap[i + bitmapRow][j+bitmapCol].cb = pel_future[i][j].cb;
					cr=myPicture->pelmap[i + bitmapRow][j+bitmapCol].cr = pel_future[i][j].cr;
				}
			}
		}
	}
}

void decode_block(macroblock* myMacroBlock, int index, picture* myPicture){
	int acIndex=0;
	//cout<<"Decode Block "<<index<<endl;
	if(myMacroBlock->macroblock_intra == 1){ // if it's an intra code macroblock/ easy pattern is 63
		if(index == 4){
			//Cb
			int size = huffmanDecode(blockSizeChrominanceRoot); // run length decode
			if(size != 0){
				int tmp_value = (int)readnBits(size);
				if( ((tmp_value >> (size -1)) & 1) == 0){ // + or - like JPEG
					tmp_value = -(tmp_value ^ fill1[size]);
				}
				myMacroBlock->myMCU.cb[0][0] = tmp_value;
			}
		}
		else if (index == 5){
			//Cr
			int size = huffmanDecode(blockSizeChrominanceRoot); //run length decode
			if(size != 0){
				int tmp_value = (int)readnBits(size);
				if( ((tmp_value >> (size -1)) & 1) == 0){ // + or - like JPEG
					tmp_value = -(tmp_value ^ fill1[size]);
				}
				myMacroBlock->myMCU.cr[0][0] = tmp_value;
			}
		}
		else{
			//Y
			//read the vlc of dct dc size luminance
			int size = huffmanDecode(blockSizeLuminanceRoot);//run length decode
			if(size != 0){ 
				int tmp_value = (int)readnBits(size);
				if( ((tmp_value >> (size -1)) & 1) == 0){ // + or - like JPEG
					tmp_value = -(tmp_value ^ fill1[size]);
				}
				myMacroBlock->myMCU.y[index][0][0] = tmp_value;
			}
		}
	}
	else{
		toDCFirst(); // the one to make the binary tree to dc first binary tree
		int run,level,signedBit;
		int result = huffmanDecode(dcCoefRoot);
		if(result > 0){ // got the stuff
			run = result >> 6;
			acIndex = run;
			level = result & 63;
			signedBit = (int)readBits();
		}
		else if(result < 0){
			if(result == -2){ //escape code
				run = (int)readnBits(6);
				level = (int)readnBits(8);
				signedBit = level >> 7;
				if((level & 127) == 0){ // if we need to read 8 more bits
					// read another 8 bit;
					level = (int)readnBits(8);
					if((level & 255) == 0){ //impossible
						fputs("Something Wrong decode Code after escape DC coef",stderr);
						exit(10);
					}
					else{
						if(signedBit==1){ // + or -
							level = level - 256;
						}
					}
				}
				else{
					if(signedBit == 1){ // + or -
						level = level - 256;
					}
				}
				signedBit=0;
			}
			else{
				fputs("wtf unknown value in DC table", stderr);
				exit(9);
			}
		}
		else{
			fputs("wtf there's 0 in DC table", stderr);
			exit(9);
		}
		//finish decode the dc coeff
		if(index == 4){//cb
			if(signedBit == 1){// + or -
				myMacroBlock-> myMCU.cb[Rowindex[acIndex]][Colindex[acIndex]] = -1*level;
			}
			else{
				myMacroBlock-> myMCU.cb[Rowindex[acIndex]][Colindex[acIndex]] = level;
			}
		}
		else if (index == 5){//cr
			if(signedBit == 1){//+ or -
				myMacroBlock-> myMCU.cr[Rowindex[acIndex]][Colindex[acIndex]] = -1*level;
			}
			else{
				myMacroBlock-> myMCU.cr[Rowindex[acIndex]][Colindex[acIndex]] = level;
			}
		}
		else{
			if(signedBit == 1){// + or =
				myMacroBlock-> myMCU.y[index][Rowindex[acIndex]][Colindex[acIndex]] = -1*level;
			}
			else{
				myMacroBlock-> myMCU.y[index][Rowindex[acIndex]][Colindex[acIndex]] = level;
			}
		}
	}
	// finish getting the first element of the stuff now get the ac coefficient
	if(myPicture->picture_coding_type != D_type_picture){
		toDCNext();
		while(1){
			int result = huffmanDecode(dcCoefRoot);
			int run,level,signedBit;
			// decode the run and level
			if(result > 0){ //got thte value
				run = result >> 6;
				level = result & 63;
				signedBit = (int)readBits();
			}
			else if(result < 0){ //escape code or EOB
				if(result == -2){ //escape code
					run = (int)readnBits(6);
					level = (int)readnBits(8);
					signedBit = level >> 7;
					if((level & 127) == 0){  //if we need to read 8 more bits or not
						// read another 8 bit;
						level = (int)readnBits(8);
						if((level & 255) == 0){
							fputs("Something Wrong decode Code after escape DC coef",stderr);
							exit(10);
						}
						else{
							if(signedBit==1){
								level = level - 256;
							}
						}
					}
					else{
						if(signedBit == 1){ //it's minus
							level = level - 256;
						}
					}
					signedBit=0;
				}
				else if (result == -1){ 
					//EOB;
					break;
				}
				else{
					fputs("wtf unknown value in DC table", stderr);
					exit(9);
				}
			}
			else{
				fputs("wtf there's 0 in DC table", stderr);
				exit(9);
			}//end of getting the run and level
			acIndex = acIndex + run +1;
			if(acIndex>63){ //get all the AC index
				fputs("WTF ac index is bigger than 63",stderr);
				exit(10);
				break;
			}
			if(index == 4){ //cb
				if(signedBit == 1){ //the decode part to check the value is positive or negative
					myMacroBlock-> myMCU.cb[Rowindex[acIndex]][Colindex[acIndex]] = -1*level;
				}
				else{
					myMacroBlock-> myMCU.cb[Rowindex[acIndex]][Colindex[acIndex]] = level;
				}
			}
			else if (index == 5){//cr
				if(signedBit == 1){ //the decodepart to check the value is positive or negative
					myMacroBlock-> myMCU.cr[Rowindex[acIndex]][Colindex[acIndex]] = -1*level;
				}
				else{
					myMacroBlock-> myMCU.cr[Rowindex[acIndex]][Colindex[acIndex]] = level;
				}
			}
			else{
				if(signedBit == 1){
					myMacroBlock-> myMCU.y[index][Rowindex[acIndex]][Colindex[acIndex]] = -1*level;
				}
				else{
					myMacroBlock-> myMCU.y[index][Rowindex[acIndex]][Colindex[acIndex]] = level;
				}
			}
		}//end of while
	}
}
void output_bmp(string title, int** bitmap){
	unsigned char _file[14] = {
    'B','M', // magic
    0,0,0,0, // size in bytes
    0,0, // app data
    0,0, // app data
    40+14,0,0,0 // start of data offset
	};
	
	unsigned char _info[40] = {
		40,0,0,0, // info hd size
		0,0,0,0, // width
		0,0,0,0, // heigth
		1,0, // number color planes
		24,0, // bits per pixel
		0,0,0,0, // compression is none
		0,0,0,0, // image bits size
		0x13,0x0B,0,0, // horz resoluition in pixel / m
		0x13,0x0B,0,0, // vert resolutions (0x03C3 = 96 dpi, 0x0B13 = 72 dpi)
		0,0,0,0, // #colors in pallete
		0,0,0,0, // #important colors
	};
	int w=(myVideo.mb_col << 4);
	int h=(myVideo.mb_row<<4);
	//cout<<"W:"<<w<<" x H: "<<h<<endl;
	int padSize  = (4-(w*3)%4)%4;
	int sizeData = w*h*3 + h*padSize;
	int sizeAll  = sizeData + sizeof(_file) + sizeof(_info);
	_file[ 2] = (unsigned char)( sizeAll    );
	_file[ 3] = (unsigned char)( sizeAll>> 8);
	_file[ 4] = (unsigned char)( sizeAll>>16);
	_file[ 5] = (unsigned char)( sizeAll>>24);

	_info[ 4] = (unsigned char)( w   );
	_info[ 5] = (unsigned char)( w>> 8);
	_info[ 6] = (unsigned char)( w>>16);
	_info[ 7] = (unsigned char)( w>>24);

	_info[ 8] = (unsigned char)( h    );
	_info[ 9] = (unsigned char)( h>> 8);
	_info[10] = (unsigned char)( h>>16);
	_info[11] = (unsigned char)( h>>24);

	_info[20] = (unsigned char)( sizeData    );
	_info[21] = (unsigned char)( sizeData>> 8);
	_info[22] = (unsigned char)( sizeData>>16);
	_info[23] = (unsigned char)( sizeData>>24);
	fstream outfile;
	outfile.open(title,ios::out| ios::binary);
	outfile.write((char*)_file, sizeof(_file));
	outfile.write((char*)_info, sizeof(_info));
	unsigned char pad[3] = {0,0,0};
	for ( int y=h-1; y>=0; y-- )
	{
		for ( int x=0; x<w; x++ )
		{
			/*long red = lround( 255.0 * waterfall[x][y] );
			if ( red < 0 ) red=0;
			if ( red > 255 ) red=255;
			long green = red;
			long blue = red;
			*/
			int mypixel = bitmap[y][x];
			int b=mypixel&255;
			int g=(mypixel>>8)&255;
			int r=(mypixel>>16)&255;
			unsigned char pixel[3];
			pixel[0] = b&255;
			pixel[1] = g&255;
			pixel[2] = r&255;

			outfile.write( (char*)pixel, 3 );
		}
		outfile.write( (char*)pad, padSize );
	}
	
}

void output_bmp2(string title, pixel** bitmap){
	unsigned char _file[14] = {
    'B','M', // magic
    0,0,0,0, // size in bytes
    0,0, // app data
    0,0, // app data
    40+14,0,0,0 // start of data offset
	};
	
	unsigned char _info[40] = {
		40,0,0,0, // info hd size
		0,0,0,0, // width
		0,0,0,0, // heigth
		1,0, // number color planes
		24,0, // bits per pixel
		0,0,0,0, // compression is none
		0,0,0,0, // image bits size
		0x13,0x0B,0,0, // horz resoluition in pixel / m
		0x13,0x0B,0,0, // vert resolutions (0x03C3 = 96 dpi, 0x0B13 = 72 dpi)
		0,0,0,0, // #colors in pallete
		0,0,0,0, // #important colors
	};
	int w=(myVideo.mb_col << 4);
	int h=(myVideo.mb_row<<4);
	//cout<<"W:"<<w<<" x H: "<<h<<endl;
	int padSize  = (4-(w*3)%4)%4;
	int sizeData = w*h*3 + h*padSize;
	int sizeAll  = sizeData + sizeof(_file) + sizeof(_info);
	_file[ 2] = (unsigned char)( sizeAll    );
	_file[ 3] = (unsigned char)( sizeAll>> 8);
	_file[ 4] = (unsigned char)( sizeAll>>16);
	_file[ 5] = (unsigned char)( sizeAll>>24);

	_info[ 4] = (unsigned char)( w   );
	_info[ 5] = (unsigned char)( w>> 8);
	_info[ 6] = (unsigned char)( w>>16);
	_info[ 7] = (unsigned char)( w>>24);

	_info[ 8] = (unsigned char)( h    );
	_info[ 9] = (unsigned char)( h>> 8);
	_info[10] = (unsigned char)( h>>16);
	_info[11] = (unsigned char)( h>>24);

	_info[20] = (unsigned char)( sizeData    );
	_info[21] = (unsigned char)( sizeData>> 8);
	_info[22] = (unsigned char)( sizeData>>16);
	_info[23] = (unsigned char)( sizeData>>24);
	fstream outfile;
	outfile.open(title,ios::out| ios::binary);
	outfile.write((char*)_file, sizeof(_file));
	outfile.write((char*)_info, sizeof(_info));
	unsigned char pad[3] = {0,0,0};
	for ( int y=h-1; y>=0; y-- )
	{
		for ( int x=0; x<w; x++ )
		{
			/*long red = lround( 255.0 * waterfall[x][y] );
			if ( red < 0 ) red=0;
			if ( red > 255 ) red=255;
			long green = red;
			long blue = red;
			*/
			//int mypixel = bitmap[y][x];
			int b=bitmap[y][x].b;
			int g=bitmap[y][x].g;
			int r=bitmap[y][x].r;
			unsigned char pixel[3];
			pixel[0] = b&255;
			pixel[1] = g&255;
			pixel[2] = r&255;

			outfile.write( (char*)pixel, 3 );
		}
		outfile.write( (char*)pad, padSize );
	}
	
}

void debugBlock(int block[8][8]){
	for(int i=0;i<8;i++){
		for (int j=0;j<8;j++){
			printf("%5d ",block[i][j]);
		}
		cout<<endl;
	}
	cout<<endl;
}