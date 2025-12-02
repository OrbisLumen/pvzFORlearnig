/*
	update log:
1.	change all the sunshine animation control from integer to float for smoother animation
2.	change sunshine ball animation implementation model from offsite to bezier curve
	abandon unnecessary sunshineball struct menbers (x,y,xoff,yoff,destY)
3.	change sound playing method several times,finally use the method alias
4.	fix there is a ghost image in last mouse clicking position when dragging plants from banner
	(need to update msg.x and msg.y when left button down in the function "userClick()")
5.	fix only one Peashooter shoot if increase createZombies frequency
	(add shootTimer for every plants instead of using one "count" for all)
6.	change when to minus sunshine
7.	add most audio by PlaySoundAsync
8.	add zombies eating sound by PlaySound,
	also by adding soundFlag and soundCount to zombie struct to control
9.	add eatingfre to zombie struct to slow zombies eating animation
10.	fix when you win the game,but the zombies are still created 
	(this is actually a variable conflict,the global zmCount meet the static zmCount)
	now change the global zmCount to zmCnt
11.	adjust zombies create frequency
	if (zmCnt <= ZM_MAX * 0.15) {zmFre = 600;}
	else if (zmCnt <= ZM_MAX * 0.75) {zmFre = rand() % 200 + 100;}
	else {zmFre = 10;}
*/

#include <stdio.h>
#include <graphics.h> // easyx graphics library
#include <time.h>
#include <math.h>
#include <tchar.h>
#include "tools.h" 
#include "vector2.h"

#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")

#define WIN_WIDTH 900
#define	WIN_HEIGHT 600
#define ZM_MAX 20 // how many zombies this game may create

enum { PeaShooter, SunFlower, PLANTS_COUNT };

IMAGE imgBg; // background image
IMAGE imgBar; // plants bar image 
IMAGE imgCards[PLANTS_COUNT]; // plant cards images
IMAGE* imgPlant[PLANTS_COUNT][20]; // plant images

int curX, curY; // selected plant card mouse position	
int selectedPlant = 0; // selected plant index 0:not selected, 1: PeaShooter, 2: SunFlower etc
bool IsDraggingPlant = 0; // whether you are dragging a plant(0: idle, 1: dragging plant card)

struct Plant {
	int type; // 0:none, 1: PeaShooter, 2: SunFlower etc	
	int frameIndex; // current frame index
	bool catched; // whether it is catched by zombies
	int deadTimer; // dead timer 

	int timer; // timer for staying on the ground
	int x, y;

	int shootTimer; // only used for shooter
};
struct Plant plantsOnField[3][9]; // plants on the field 

enum { SUNSHINE_DOWN, SUNSHINE_GROUND, SUNSHINE_COLLECT, SUNSHINE_PRODUCT };

struct sunshineBall {

	// abandon previous x,y,destY,xoff,yoff 
	// all change to bezier curve

	//float x, y; // position in falldown animation(x not changed)
	int frameIndex; // current picture frame index
	//int destY; // destination y position
	bool used; // whether this sunshine ball is used
	int timer; // timer for staying at destY 

	//float xoff; // x offset for collecting animation
	//float yoff; // y offset for collecting animation

	float t; // time point for bezier curve (0 to 1)
	vector2 p1, p2, p3, p4; // control points for bezier curve
	vector2 pCur; // current position 
	float speed; // speed in bezier curve 
	int state; // sunshine ball state (falling, on ground, collecting, produced)
};
IMAGE sunshineBallImg[29]; // sunshine ball images
int sunshine;
struct sunshineBall sunshineBalls[10]; // sunshine balls pool
// use the pool to store sunshine balls (the concept of a pool is from backend development)

struct zombie {
	int x, y;
	int frameIndex;
	bool used;
	int speed;
	int row;
	int hp;
	bool dead;
	bool eating; // whether it is eating a plant

	bool soundFlag; // whether start to play eating audio
	int soundCount; // sound timer

	int eatingfre; // slow eating
};
struct zombie zombies[10]; // zombies pool
IMAGE imgZombies[22]; // zombie images
IMAGE imgZombieDead[20]; // zombie dead images
IMAGE imgZombieEat[21]; // zombie eat images
IMAGE imgZombieStand[11]; // zombie stand images(before the game really start)

struct bullet {
	int x, y;
	int row;
	bool used;
	int speed;
	bool blast; // whether it hits a target
	int frameIndex;
};
struct bullet bullets[30]; // bullets pool
IMAGE imgBulletNormal;
IMAGE imgBulletBlast[4]; // bullet blast images

enum { GOING, WIN, FAIL };
int killCount; // how many zombies are killed currently
int zmCnt; // how many zombies have been created already
int gameStatus;





DWORD WINAPI PlaySoundInBackground(LPVOID lpParam) {
	const char* file = (const char*)lpParam;

	char cmd[256];
	static int id = 0;   // give every call a different alias
	int myId = id++;

	sprintf(cmd, "open \"%s\" type mpegvideo alias snd%d", file, myId);
	mciSendString(cmd, NULL, 0, NULL);

	sprintf(cmd, "play snd%d wait", myId);
	mciSendString(cmd, NULL, 0, NULL);

	sprintf(cmd, "close snd%d", myId);
	mciSendString(cmd, NULL, 0, NULL);

	return 0;

	/*const char* soundFile = (const char*)lpParam;
	mciSendString(soundFile, 0, 0, 0);
	return 0;*/
}// create a thread to play sound

void PlaySoundAsync(const char* soundFile) {
	CreateThread(NULL, 0, PlaySoundInBackground, (LPVOID)soundFile, 0, NULL);
}// package sound playing function

bool fileExist(const char* name) {
	FILE* fp = fopen(name, "r");
	if (fp == NULL) {
		return false;
	}
	else {
		fclose(fp);
		return true;
	}

}// check if a file exists

void gameInit() {

	// init game status
	killCount = 0;
	zmCnt = 0;
	gameStatus = GOING;

	// load image
	loadimage(&imgBg, "res/bg.jpg");
	//load bar
	loadimage(&imgBar, "res/bar.png");

	//init imgPlant array
	memset(imgPlant, 0, sizeof(imgPlant));
	//init plantsOnField array
	memset(plantsOnField, 0, sizeof(plantsOnField));

	//load plant cards
	char name[64];
	for (int i = 0; i < PLANTS_COUNT; i++) {
		// generate plant card file name
		sprintf_s(name, sizeof(name), "res/Cards/card_%d.png", i + 1);
		loadimage(&imgCards[i], name);

		// load plant images
		for (int j = 0; j < 20; j++) {
			sprintf_s(name, sizeof(name), "res/Plants/%d/%d.png", i, j + 1);
			if (fileExist(name)) {
				imgPlant[i][j] = new IMAGE;
				loadimage(imgPlant[i][j], name);
			}
			else {
				break;
			}

		}
	}

	selectedPlant = 0;
	sunshine = 50;

	// init sunshine balls pool
	memset(sunshineBalls, 0, sizeof(sunshineBalls));

	// load sunshine ball images
	for (int i = 0; i < 29; i++) {
		sprintf_s(name, sizeof(name), "res/sunshine/%d.png", i + 1);
		loadimage(&sunshineBallImg[i], name);
	}

	// seed random number generator
	srand(time(NULL));

	// create window
	initgraph(WIN_WIDTH, WIN_HEIGHT, 0);

	// set font style
	LOGFONT f;
	gettextstyle(&f);
	f.lfHeight = 30;
	f.lfWeight = 15;
	strcpy(f.lfFaceName, "Segoe UI Black");
	f.lfQuality = ANTIALIASED_QUALITY; // enable anti-aliasing
	settextstyle(&f);
	setbkmode(TRANSPARENT); // set text background mode to transparent
	setcolor(BLACK);

	// init zombies pool
	memset(zombies, 0, sizeof(zombies));

	// load zombie images
	for (int i = 0; i < 22; i++) {
		sprintf_s(name, sizeof(name), "res/zm/%d.png", i + 1);
		loadimage(&imgZombies[i], name);
	}

	// load zombies dead images
	for (int i = 0; i < 20; i++) {
		sprintf_s(name, sizeof(name), "res/zm_dead/%d.png", i + 1);
		loadimage(&imgZombieDead[i], name);
	}

	// load zombies eat images
	for (int i = 0; i < 21; i++) {
		sprintf_s(name, sizeof(name), "res/zm_eat/%d.png", i + 1);
		loadimage(&imgZombieEat[i], name);
	}

	// load zombies stand images
	for (int i = 0; i < 11; i++) {
		sprintf_s(name, sizeof(name), "res/zm_stand/%d.png", i + 1);
		loadimage(&imgZombieStand[i], name);
	}

	// init bullets pool
	memset(bullets, 0, sizeof(bullets));

	// load normal bulllet image
	loadimage(&imgBulletNormal, "res/bullets/bullet_normal.png");

	// init bullet blast images
	loadimage(&imgBulletBlast[3], "res/bullets/bullet_blast.png");
	for (int i = 0; i < 3; i++) {
		float k = (i + 1) * 0.25;
		loadimage(&imgBulletBlast[i], "res/bullets/bullet_blast.png",
			imgBulletBlast[3].getwidth() * k,
			imgBulletBlast[3].getheight() * k,
			true);
	}
}

void startUI() {
	mciSendString("play res/sound/bg.mp3 repeat", 0, 0, 0);
	IMAGE imgStartBg, imgMenu1, imgMenu2;
	loadimage(&imgStartBg, "res/menu.png");
	loadimage(&imgMenu1, "res/menu1.png");
	loadimage(&imgMenu2, "res/menu2.png");
	int flag = 0;

	while (1) {
		BeginBatchDraw();
		putimage(0, 0, &imgStartBg);
		putimagePNG(474, 75, flag ? &imgMenu2 : &imgMenu1);

		ExMessage msg;
		if (peekmessage(&msg)) {
			if (msg.message == WM_LBUTTONDOWN &&
				msg.x > 474 && msg.x < 474 + 300 &&
				msg.y>75 && msg.y < 75 + 140) {
				flag = 1;
			}
			else if (msg.message == WM_LBUTTONUP && flag) {
				PlaySoundAsync("res/sound/buttonclick.mp3");
				Sleep(600);
				PlaySoundAsync("res/sound/evillaugh.mp3");

				// play click animation
				for (int i = 0; i < 6; i++) {
					flag = 1 - flag;

					BeginBatchDraw();
					putimage(0, 0, &imgStartBg);
					putimagePNG(474, 75, flag ? &imgMenu2 : &imgMenu1);
					EndBatchDraw();

					Sleep(200);
				}
				Sleep(3000);
				mciSendString("close res/sound/bg.mp3", 0, 0, 0);
				mciSendString("play res/sound/MainBg.mp3 repeat", 0, 0, 0);
				return;
			}
		}

		EndBatchDraw();
	}
}

void viewScence() {
	int xMin = WIN_WIDTH - imgBg.getwidth(); // 900 - 1400 = -500
	vector2 points[9] = {
		{550,80},{530,160},{630,170},{530,200},{515,270},
		{565,370},{605,340},{705,280},{690,340}
	}; // fixed position for stand zombies
	int index[9]; // random initial frame index for stand zombies
	for (int i = 0; i < 9; i++) {
		index[i] = rand() % 11;
	}
	int count = 0;
	for (int x = 0; x > xMin; x -= 3) {
		BeginBatchDraw();
		putimage(x, 0, &imgBg);
		count++;
		for (int k = 0; k < 9; k++) {
			putimagePNG(points[k].x - xMin + x, points[k].y, &imgZombieStand[index[k]]);
			// every 5 frame update zombies
			if (count >= 5) {
				index[k] = (index[k] + 1) % 11;
			}
		}
		if (count >= 5)count = 0;

		EndBatchDraw();
		Sleep(5);
	}

	// stay for few seconds
	for (int i = 0; i < 100; i++) {
		BeginBatchDraw();

		putimage(xMin, 0, &imgBg);
		for (int k = 0; k < 9; k++) {
			putimagePNG(points[k].x, points[k].y, &imgZombieStand[index[k]]);
			index[k] = (index[k] + 1) % 11;
		}
		EndBatchDraw();
		Sleep(20);
	}

	for (int x = xMin; x < -112; x += 3) {
		BeginBatchDraw();
		putimage(x, 0, &imgBg);
		for (int k = 0; k < 9; k++) {
			putimagePNG(points[k].x - xMin + x, points[k].y, &imgZombieStand[k]);
		}// when scene start sliding,zombies stop stand animation

		EndBatchDraw();

		Sleep(5);
	}
}

void barsDown() {
	int height = imgBar.getheight();
	for (int i = -height; i <= 0; i += 4) {
		BeginBatchDraw();

		putimage(-112, 0, &imgBg);
		putimagePNG(250, i, &imgBar);

		for (int j = 0; j < PLANTS_COUNT; j++) {
			int x = 338 + j * 65;
			int y = 6 + i;
			putimagePNG(x, y, &imgCards[j]);
		}

		EndBatchDraw();
		Sleep(10);
	}
}

void drawStage() {
	putimage(-112, 0, &imgBg);
	putimagePNG(250, 0, &imgBar);
}

void createSunshineFromUpperWindowEdge() {

	static int count = 0;
	static int fre = 400; // initial frequency
	count++;

	if (count >= fre) {
		fre = 400 + rand() % 200; // random frequency between 400 and 600 frames
		count = 0;

		// get sunshine ball that is not used from pool
		int ballMax = sizeof(sunshineBalls) / sizeof(sunshineBalls[0]);

		int i;
		for (i = 0; i < ballMax && sunshineBalls[i].used; i++);
		if (i >= ballMax) {
			return; // no available sunshine ball
		}
		sunshineBalls[i].used = true;
		sunshineBalls[i].frameIndex = 0;
		//sunshineBalls[i].x = 260 + rand() % (860 - 260);
		//sunshineBalls[i].y = 60;
		//sunshineBalls[i].destY = 200 + (rand() % 4) * 90;
		sunshineBalls[i].timer = 0;
		//sunshineBalls[i].xoff = 0;
		//sunshineBalls[i].yoff = 0;
		sunshineBalls[i].state = SUNSHINE_DOWN;
		sunshineBalls[i].t = 0;
		sunshineBalls[i].p1 = vector2(260 - 112 + rand() % (900 - (260 - 112)), 60);
		sunshineBalls[i].p4 = vector2(sunshineBalls[i].p1.x, 200 + (rand() % 4) * 90);
		int off = 2;
		float distance = sunshineBalls[i].p4.y - sunshineBalls[i].p1.y;
		sunshineBalls[i].speed = 1.0 / (distance / off);

		// I wonder whether the bezier curve algorithm here more easier ?

	}

}

void createSunshineFromSunFlower() {

	int ballMax = sizeof(sunshineBalls) / sizeof(sunshineBalls[0]);

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 9; j++) {
			if (plantsOnField[i][j].type == SunFlower + 1) {
				plantsOnField[i][j].timer++;
				if (plantsOnField[i][j].timer > 500) {
					plantsOnField[i][j].timer = 0;

					int k;
					for (k = 0; k < ballMax && sunshineBalls[k].used; k++);
					if (k >= ballMax) return;

					sunshineBalls[k].used = true;
					sunshineBalls[k].p1 = vector2(plantsOnField[i][j].x, plantsOnField[i][j].y);
					int w = (100 + rand() % 50) * (rand() % 2 ? 1 : -1); // bug? generate desitination out of window
					sunshineBalls[k].p4 = vector2(plantsOnField[i][j].x + w,
						plantsOnField[i][j].y + imgPlant[SunFlower][0]->getwidth() - sunshineBallImg[0].getwidth());
					sunshineBalls[k].p2 = vector2(sunshineBalls[k].p1.x + w * 0.3, sunshineBalls[k].p1.y - 100);
					sunshineBalls[k].p3 = vector2(sunshineBalls[k].p1.x + w * 0.7, sunshineBalls[k].p1.y - 100);
					sunshineBalls[k].state = SUNSHINE_PRODUCT;
					sunshineBalls[k].speed = 0.05;
					sunshineBalls[k].t = 0;
				}
			}
		}
	}
}

void createSunshine() {

	// randomly generate sunshine ball from upper window edge
	createSunshineFromUpperWindowEdge();

	// generate sunshine ball from sunflower 
	createSunshineFromSunFlower();

}

void updateSunshine() {
	int ballMax = sizeof(sunshineBalls) / sizeof(sunshineBalls[0]);
	for (int i = 0; i < ballMax; i++) {
		if (sunshineBalls[i].used) {
			sunshineBalls[i].frameIndex = (sunshineBalls[i].frameIndex + 1) % 29;
			if (sunshineBalls[i].state == SUNSHINE_DOWN) {
				sunshineBalls[i].t += sunshineBalls[i].speed;
				sunshineBalls[i].pCur = sunshineBalls[i].p1 +
					sunshineBalls[i].t * (sunshineBalls[i].p4 - sunshineBalls[i].p1); // 2 points bezier curve 
				if (sunshineBalls[i].t >= 1) {
					sunshineBalls[i].state = SUNSHINE_GROUND;
					sunshineBalls[i].timer = 0;
				}
			}
			else if (sunshineBalls[i].state == SUNSHINE_GROUND) {
				sunshineBalls[i].timer++;
				if (sunshineBalls[i].timer > 100) {
					sunshineBalls[i].used = false;
					sunshineBalls[i].timer = 0;
				}
			}
			else if (sunshineBalls[i].state == SUNSHINE_COLLECT) {
				sunshineBalls[i].t += sunshineBalls[i].speed;
				sunshineBalls[i].pCur = sunshineBalls[i].p1 +
					sunshineBalls[i].t * (sunshineBalls[i].p4 - sunshineBalls[i].p1); // 2 points bezier curve
				if (sunshineBalls[i].t > 1) {
					sunshineBalls[i].used = false;
					sunshine += 25;
				}
			}
			else if (sunshineBalls[i].state == SUNSHINE_PRODUCT) {
				sunshineBalls[i].t += sunshineBalls[i].speed;
				sunshineBalls[i].pCur = calcBezierPoint(sunshineBalls[i].t, sunshineBalls[i].p1, sunshineBalls[i].p2, sunshineBalls[i].p3, sunshineBalls[i].p4);
				// use Bezier Curve
				if (sunshineBalls[i].t > 1) {
					sunshineBalls[i].state = SUNSHINE_GROUND;
					sunshineBalls[i].timer = 0;
				}
			}
		}

		//if (sunshineBalls[i].used) {

		//	sunshineBalls[i].frameIndex = (sunshineBalls[i].frameIndex + 1) % 29;
		//	if (sunshineBalls[i].timer == 0) {
		//		sunshineBalls[i].y += 2; // fall down speed
		//	}

		//	if (sunshineBalls[i].y >= sunshineBalls[i].destY) {
		//		sunshineBalls[i].timer++;

		//		if (sunshineBalls[i].timer >= 100) {
		//			sunshineBalls[i].used = false; // lose the sunshine ball	
		//		}
		//	}

		//}

		//else if (sunshineBalls[i].xoff) {
		//	sunshineBalls[i].x -= sunshineBalls[i].xoff;
		//	sunshineBalls[i].y -= sunshineBalls[i].yoff;
		//	if (sunshineBalls[i].x < 262 || sunshineBalls[i].y < 0) {
		//		sunshineBalls[i].xoff = 0;
		//		sunshineBalls[i].yoff = 0;
		//		sunshine += 25;
		//	}
		//}
	}
}

void drawSunshine() {
	int ballMax = sizeof(sunshineBalls) / sizeof(sunshineBalls[0]);
	for (int i = 0; i < ballMax; i++) {
		// render sunshineballs while floating or collecting 
		//	if (sunshineBalls[i].used || sunshineBalls[i].xoff) 
		if (sunshineBalls[i].used) {  // still some problems here later to solve
			IMAGE* img = &sunshineBallImg[sunshineBalls[i].frameIndex];
			//putimagePNG(sunshineBalls[i].x, sunshineBalls[i].y, img);
			putimagePNG(sunshineBalls[i].pCur.x, sunshineBalls[i].pCur.y, img);
		}
	}


}

void collectSunshine(ExMessage* msg) {
	int ballMax = sizeof(sunshineBalls) / sizeof(sunshineBalls[0]);
	for (int i = 0; i < ballMax; i++) {
		if (sunshineBalls[i].used) {
			//int x = sunshineBalls[i].x;
			//int y = sunshineBalls[i].y;
			int x = sunshineBalls[i].pCur.x;
			int y = sunshineBalls[i].pCur.y;

			if (msg->x > x && msg->x<x + sunshineBallImg[0].getwidth() &&
				msg->y > y && msg->y < y + sunshineBallImg[0].getheight()) {

				//sunshineBalls[i].used = false; // collect the sunshine ball

				sunshineBalls[i].state = SUNSHINE_COLLECT;

				PlaySoundAsync("res/sound/sun_collect.mp3");

				// set sunshine ball offset for collecting animation
				//float destY = 0; // y position of sunshine counter
				//float destX = 262; // x position of sunshine counter	
				//float angle = atan((y - destY) / (x - destX));
				//sunshineBalls[i].xoff = 8 * cos(angle);
				//sunshineBalls[i].yoff = 8 * sin(angle);

				// now use bezier curve for collecting animation
				sunshineBalls[i].p1 = sunshineBalls[i].pCur;
				sunshineBalls[i].p4 = vector2(262, 0);
				sunshineBalls[i].t = 0;
				float distance = dis(sunshineBalls[i].p1 - sunshineBalls[i].p4);
				float off = 16;
				sunshineBalls[i].speed = 1.0 / (distance / off); // 1 frame move 8 px

				break; // collet one sunshine ball once
			}
		}
	}
}

void drawSunshinePoints() {
	char scoreText[32];
	sprintf_s(scoreText, sizeof(scoreText), "%d", sunshine);
	
	if (sunshine == 0) {
		outtextxy(290, 67, scoreText);
	}
	else if (sunshine < 100) {
		outtextxy(279, 67, scoreText);
	}
	else {
		outtextxy(276, 67, scoreText);
	}
}

void updatePlants() {

	// for change plants overall animation speed
	static int count = 0;
	if (++count < 2)return;
	count = 0;

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 9; j++) {
			if (plantsOnField[i][j].type > 0) {
				plantsOnField[i][j].frameIndex++;
				int type = plantsOnField[i][j].type - 1;
				int index = plantsOnField[i][j].frameIndex;
				if (imgPlant[type][index] == NULL) {
					plantsOnField[i][j].frameIndex = 0;
				}
			}
		}
	}
}

void drawPlants() {
	// render plants on the field	
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 9; j++) {
			if (plantsOnField[i][j].type > 0) {

				//int x = 256 + j * 81;
				//int y = 179 + i * 102 + 14;
				int type = plantsOnField[i][j].type - 1;
				int index = plantsOnField[i][j].frameIndex;
				//putimagePNG(x, y, imgPlant[type][index]);
				putimagePNG(plantsOnField[i][j].x, plantsOnField[i][j].y, imgPlant[type][index]);

			}
		}
	}

	// render dragging plant card
	if (IsDraggingPlant) {
		IMAGE* img = imgPlant[selectedPlant - 1][0];
		putimagePNG(curX - img->getwidth() / 2, curY - img->getheight() / 2, img);
	}

}

void drawPlantsCards() {
	for (int i = 0; i < PLANTS_COUNT; i++) {
		int x = 338 + i * 65;
		int y = 5;
		putimagePNG(x, y, &imgCards[i]);
	}
}

void createZombies() {
	if (zmCnt >= ZM_MAX) { 
		return; 
	}
	static int zmFre = 1200; // initial frequency
	static int zmCount = 0;
	static bool soundFlag = true; // use for the first zombie
	static bool soundFlagFinalWave = true; // use for the final wave audio
	char name[64];
	zmCount++;

	// printf("zmCount: %d, zmFre: %d\n", zmCount, zmFre); // for debug

	if (zmCount > zmFre) {
		if (soundFlag) {
			PlaySoundAsync("res/sound/awooga.mp3");
			soundFlag = false;
		}
		else if(zmCnt <= ZM_MAX * 0.75){
			int randomNum = rand() % 6 + 1;
			switch (randomNum) {
			case(1): {
				Sleep(100);
				PlaySoundAsync("res/sound/groan1.mp3"); break;
			}
			case(2): {
				Sleep(100);
				PlaySoundAsync("res/sound/groan2.mp3"); break;
			}
			case(3): {
				Sleep(100);
				PlaySoundAsync("res/sound/groan3.mp3"); break;
			}
			case(4): {
				Sleep(100);
				PlaySoundAsync("res/sound/groan4.mp3"); break;
			}
			case(5): {
				Sleep(100);
				PlaySoundAsync("res/sound/groan5.mp3"); break;
			}
			case(6): {
				Sleep(100);
				PlaySoundAsync("res/sound/groan6.mp3"); break;
			}
			}
		}
		zmCount = 0;

		if (zmCnt <= ZM_MAX * 0.15) {
			zmFre = 600;
		}
		else if (zmCnt <= ZM_MAX * 0.75) {
			zmFre = rand() % 200 + 100;
		}
		else {
			if (soundFlagFinalWave) {
				PlaySoundAsync("res/sound/awooga.mp3");
				soundFlagFinalWave = false;
			}
			zmFre = 10;
		}

		int zombiesMax = sizeof(zombies) / sizeof(zombies[0]);
		for (int i = 0; i < zombiesMax; i++) {
			if (!zombies[i].used) {
				memset(&zombies[i], 0, sizeof(zombies[i]));
				zombies[i].used = true;
				zombies[i].x = WIN_WIDTH;
				zombies[i].row = rand() % 3;
				zombies[i].y = 172 + (1 + zombies[i].row) * 100;
				zombies[i].speed = 1;
				zombies[i].hp = 100;
				zombies[i].dead = false;
				zmCnt++;
				break; // Stop once spawn a new zombie
			}
		}
	}

}

void updateZombies() {
	int zombiesMax = sizeof(zombies) / sizeof(zombies[0]);

	// for change zombies overall animation speed
	static int count = 0;
	if (++count > 2)return;
	count = 0;

	// update position
	static int positioncount = 0; // position update frequency control
	positioncount++;
	if (positioncount > 3) {
		for (int i = 0; i < zombiesMax; i++) {
			if (zombies[i].used) {
				zombies[i].x -= zombies[i].speed;
				if (zombies[i].x < 170 - 112) {
					// zombie reaches the house, game over
					//printf("Game Over\n");
					//MessageBox(NULL, "over", "over", 0); // add proper game over screen later
					//exit(0); // perfect it later
					gameStatus = FAIL;
				}
			}
		}
	}

	// update frame index
	static int framecount = 0;

	framecount++;

	if (framecount > 8) {
		for (int i = 0; i < zombiesMax; i++) {
			if (zombies[i].used) {
				if (zombies[i].dead) {
					zombies[i].frameIndex++;
					if (zombies[i].frameIndex >= 20) {
						zombies[i].used = false; // give back to pool
						killCount++;
						if (killCount == ZM_MAX) {
							gameStatus = WIN;
						}
					}
				}
				else if (zombies[i].eating) {
					zombies[i].eatingfre++;
					if (zombies[i].eatingfre > 1) {
						zombies[i].eatingfre = 0;
						zombies[i].frameIndex = (zombies[i].frameIndex + 1) % 21;
					}
				}
				else {
					zombies[i].frameIndex = (zombies[i].frameIndex + 1) % 22;
				}

			}
		}
	}
}

void drawZombies() {
	int zombiesMax = sizeof(zombies) / sizeof(zombies[0]);
	for (int i = 0; i < zombiesMax; i++) {
		if (zombies[i].used) {
			// IMAGE* img = (zombies[i].dead) ? imgZombieDead : imgZombies;
			IMAGE* img = NULL;
			if (zombies[i].dead)img = imgZombieDead;
			else if (zombies[i].eating)img = imgZombieEat;
			else img = imgZombies;
			img += zombies[i].frameIndex;

			putimagePNG(zombies[i].x, zombies[i].y - img->getheight(), img);
		}
	}
}

void createBullets() {
	int lines[3] = { 0 }; // 0: no zombies this line, 1: zombies exist this line
	int zombiesMax = sizeof(zombies) / sizeof(zombies[0]);
	int bulletsMax = sizeof(bullets) / sizeof(bullets[0]);
	int dangerX = WIN_WIDTH - imgZombies[0].getwidth() / 4; // when zombie x < dangerX, peashooter shoots bullet

	for (int i = 0; i < zombiesMax; i++) {
		if (zombies[i].used && zombies[i].x < dangerX) {
			lines[zombies[i].row] = 1;
		}
	}

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 9; j++) {
			if (plantsOnField[i][j].type == PeaShooter + 1 && lines[i]) {
				//static int count = 0;
				//count++;
				plantsOnField[i][j].shootTimer++;
				if (plantsOnField[i][j].shootTimer > 50) {
					//count = 0;
					plantsOnField[i][j].shootTimer = 0;
					// find bullet from pool		
					int k;
					for (k = 0; k < bulletsMax && bullets[k].used; k++);
					if (k < bulletsMax) {
						bullets[k].used = true;
						bullets[k].row = i;
						bullets[k].speed = 4;

						bullets[k].blast = false;
						bullets[k].frameIndex = 0;

						int plantX = 256 - 112 + j * 81;
						int plantY = 179 + i * 102 + 14;
						bullets[k].x = plantX + imgPlant[PeaShooter][0]->getwidth() - 10;
						bullets[k].y = plantY + 5;
					}
				}
			}
		}
	}
}

void updateBullets() {
	int bulletsMax = sizeof(bullets) / sizeof(bullets[0]);
	for (int i = 0; i < bulletsMax; i++) {
		if (bullets[i].used) {
			bullets[i].x += bullets[i].speed;
			if (bullets[i].x > WIN_WIDTH) {
				bullets[i].used = false;
			}

			if (bullets[i].blast) {
				bullets[i].frameIndex++;
				if (bullets[i].frameIndex >= 4) {
					bullets[i].used = false;
				}
			}
		}
	}
}

void checkBullets2Zombies() {
	int bulletMax = sizeof(bullets) / sizeof(bullets[0]);
	int zombieMax = sizeof(zombies) / sizeof(zombies[0]);
	for (int i = 0; i < bulletMax; i++) {
		if (bullets[i].used == false || bullets[i].blast) continue;
		for (int j = 0; j < zombieMax; j++) {
			if (zombies[j].used == false) continue;
			int x1 = zombies[j].x + 80;
			int x2 = zombies[j].x + 110;
			int x = bullets[i].x;
			if (zombies[j].dead == false && bullets[i].row == zombies[j].row && x > x1 && x < x2) {
				PlaySoundAsync("res/sound/splat.mp3");
				zombies[j].hp -= 20; // 5 bullets to kill a zombie
				bullets[i].blast = true;
				bullets[i].speed = 0;

				if (zombies[j].hp <= 0) {
					zombies[j].dead = true;
					zombies[j].frameIndex = 0;
					zombies[j].speed = 0;
				}

				break; // one bullet hits only one zombie
			}
		}
	}
}

void checkZombies2Plants() {
	int zombiesMax = sizeof(zombies) / sizeof(zombies[0]);

	for (int i = 0; i < zombiesMax; i++) {
		if (zombies[i].used == false) continue;
		if (zombies[i].dead) continue;
		int row = zombies[i].row;
		for (int j = 0; j < 9; j++) {
			if (plantsOnField[row][j].type == 0) continue;
			int plantX = 256 - 112 + j * 81;
			int x1 = plantX + 10; // displayed plant left border
			int x2 = plantX + 60; // displayed plant right border
			int x3 = zombies[i].x + 80; // displayed zombie left border
			if (x3 > x1 && x3 < x2) {

				// the algorithm needs to be optimized here 
				// the design here seems very poor

				if (plantsOnField[row][j].catched) {
					plantsOnField[row][j].deadTimer++;

					zombies[i].soundCount++;
					zombies[i].eating = true;
					zombies[i].speed = 0;

					if (plantsOnField[row][j].deadTimer > 100) {
						plantsOnField[row][j].deadTimer = 0;
						plantsOnField[row][j].type = 0;
						zombies[i].eating = false;
						zombies[i].soundFlag = false;
						zombies[i].frameIndex = 0;
						zombies[i].speed = 1;
					}

				}
				else {
					plantsOnField[row][j].catched = true;
					zombies[i].eating = true;
					zombies[i].soundFlag = true;
					zombies[i].soundCount = 0;
					zombies[i].speed = 0;
					zombies[i].frameIndex = 0;
				}
			}
		}
		if (zombies[i].soundFlag && zombies[i].soundCount > 25) {
			zombies[i].soundCount = 0;
			PlaySound("res/sound/chomp.wav", NULL, SND_FILENAME | SND_ASYNC);

		}
	}

}

void collisionCheck() {
	// this is collision check function

	// check whether bullets hit zombies
	checkBullets2Zombies();

	// check whether zombies catch plants
	checkZombies2Plants();

}

void drawBullets() {
	int bulletMax = sizeof(bullets) / sizeof(bullets[0]);
	for (int i = 0; i < bulletMax; i++) {
		if (bullets[i].used) {
			if (bullets[i].blast) {
				IMAGE* img = &imgBulletBlast[bullets[i].frameIndex];
				putimagePNG(bullets[i].x, bullets[i].y, img);

			}
			else
			{
				putimagePNG(bullets[i].x, bullets[i].y, &imgBulletNormal);
			}
		}
	}

}

void userClick() {
	ExMessage msg;
	//static int status = 0; // 0: idle, 1: dragging plant card	
	if (peekmessage(&msg)) {
		if (msg.message == WM_LBUTTONDOWN) {

			// add to fix ghost image problem
			curX = msg.x;
			curY = msg.y;

			if (msg.x > 338 && msg.x < 338 + 65 * PLANTS_COUNT && msg.y>5 && msg.y < 5 + 90) {
				int index = (msg.x - 338) / 65;
				selectedPlant = index + 1;
				switch (selectedPlant) {
				case(PeaShooter + 1): {
					if (sunshine >= 100) {
						PlaySoundAsync("res/sound/coffee.mp3");
						//sunshine -= 100;
						IsDraggingPlant = 1; // start dragging plant card
					}
					else {
						PlaySoundAsync("res/sound/puff.mp3");
						IsDraggingPlant = 0;
					}
					break;
				}
				case(SunFlower + 1): {
					if (sunshine >= 50) {
						PlaySoundAsync("res/sound/coffee.mp3");
						//sunshine -= 50;
						IsDraggingPlant = 1; // start dragging plant card
					}
					else {
						PlaySoundAsync("res/sound/puff.mp3");
						IsDraggingPlant = 0;
					}
					break;
				}

				default: { break; }
				}
			}
			else {
				collectSunshine(&msg);
			}
		}
		else if (msg.message == WM_MOUSEMOVE && IsDraggingPlant == 1) {
			curX = msg.x;
			curY = msg.y;


		}
		else if (msg.message == WM_LBUTTONUP && IsDraggingPlant == 1) {
			if (msg.x > 256 - 112 && msg.y > 179 && msg.y < 489) {
				int row = (msg.y - 179) / 102;
				int col = (msg.x - (256 - 112)) / 81;

				if (plantsOnField[row][col].type == 0) {
					PlaySoundAsync("res/sound/plant1.mp3");
					plantsOnField[row][col].type = selectedPlant;
					plantsOnField[row][col].frameIndex = 0;
					plantsOnField[row][col].catched = false;
					plantsOnField[row][col].deadTimer = 0;

					plantsOnField[row][col].x = 256 - 112 + col * 81;
					plantsOnField[row][col].y = 179 + row * 102 + 14;
					plantsOnField[row][col].timer = 0;
					plantsOnField[row][col].shootTimer = 0;

					switch (selectedPlant) {
					case(PeaShooter + 1): {
						sunshine -= 100;
						IsDraggingPlant = 0; // cancel dragging plant card

						break;
					}
					case(SunFlower + 1): {
						sunshine -= 50;
						IsDraggingPlant = 0; // cancel dragging plant card

						break;
					}
					default: { break; }
					}

				}
				//else {
				//	switch (selectedPlant) {
				//	case(PeaShooter + 1): {
				//		sunshine += 100;
				//		IsDraggingPlant = 0; // cancel dragging plant card

				//		break;
				//	}
				//	case(SunFlower + 1): {
				//		sunshine += 50;
				//		IsDraggingPlant = 0; // cancel dragging plant card

				//		break;
				//	}

				//	default: { break; }
				//	}
				//}
			}
			//else {
			//	switch (selectedPlant) {
			//	case(PeaShooter + 1): {
			//		sunshine += 100;
			//		IsDraggingPlant = 0; // cancel dragging plant card

			//		break;
			//	}
			//	case(SunFlower + 1): {
			//		sunshine += 50;
			//		IsDraggingPlant = 0; // cancel dragging plant card

			//		break;
			//	}

			//	default: { break; }
			//	}
			//}


			selectedPlant = 0;
			IsDraggingPlant = 0;

		}
	}
}

void updateWindow() {

	//start double buffering
	BeginBatchDraw();

	// render background and UI
	drawStage();

	// render banner plant cards
	drawPlantsCards();

	// render plants (dragging and on the field)
	drawPlants();

	// render sunshine balls
	drawSunshine();

	// render sunshine points
	drawSunshinePoints();

	// render zombies
	drawZombies();

	// render bullets
	drawBullets();

	//finish double buffering
	EndBatchDraw();
}

void updateGame() {

	//update plants
	updatePlants();

	// create sunshine balls randomly
	createSunshine();

	// update sunshine balls
	updateSunshine();

	// create zombies randomly
	createZombies();

	// update zombies
	updateZombies();

	// create bullets
	createBullets();

	// update bullets
	updateBullets();

	// check bullet-zombie collisions
	collisionCheck();

}

bool checkOver() {
	int ret = false;
	if (gameStatus == WIN) {
		Sleep(200);
		loadimage(0, "res/win.png");
		PlaySoundAsync("res/sound/win.mp3");
		ret = true;
	}
	else if (gameStatus == FAIL) {
		PlaySoundAsync("res/sound/scream.mp3");
		Sleep(3000);
		loadimage(0, "res/fail.png");
		PlaySoundAsync("res/sound/lose.mp3");
		ret = true;
	}
	return ret;
}

int main(void) {

	// init all the data
	gameInit();

	// this is the lobby
	startUI();

	// when click the start button,play zhe trasition animation
	viewScence();

	// upper bars slowly down into window
	barsDown();

	int timer = 0; // refresh timer
	bool flag = true; // refresh valve

	while (1) {
		userClick();
		timer += getDelay();
		if (timer >= 20) {
			timer = 0;
			flag = true;
		}
		if (flag) {
			flag = false;
			updateGame();
			updateWindow();
			if (checkOver()) {
				mciSendString("close res/sound/MainBg.mp3", 0, 0, 0);
				break;
			}
		}
	}

	system("pause");
	return 0;
}