/*
    Interactive Moon Phases System

    Controls:
    - Mouse Left Click : Select / enlarge a moon phase
    - Space            : Pause / Resume animation
    - Arrow Keys       : Rotate camera
    - + / -            : Zoom in / Zoom out
    - R                : Reset camera
    - ESC              : Exit
*/

#define _CRT_SECURE_NO_WARNINGS

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/glu.h>
#endif

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#define PI 3.14159265358979323846
#define PHASE_COUNT 8
#define STAR_COUNT 900

using namespace std;

// =========================================================
// Structures
// =========================================================

struct Color {
    float r, g, b;
};

struct MoonPhase {
    const char* name;
    const char* info;
    float angle;
    Color color;
};

// =========================================================
// Global Variables
// =========================================================

MoonPhase phases[PHASE_COUNT];

int selectedPhase = -1;
bool isPaused = false;

float cameraDistance = 28.0f;
float cameraAngleX = 24.0f;
float cameraAngleY = -32.0f;

int windowWidth = 1200;
int windowHeight = 720;

GLUquadric* sphereQuadric = NULL;

GLuint earthTextureId = 0;
GLuint moonTextureId = 0;

bool earthHasTexture = false;
bool moonHasTexture = false;

float earthSelfAngle = 0.0f;
float moonOrbitAngle = 0.0f;
float moonSelfAngle = 0.0f;

float moonOrbitRadius = 8.0f;

float starX[STAR_COUNT], starY[STAR_COUNT], starZ[STAR_COUNT];
float starBrightness[STAR_COUNT];

// =========================================================
// Helper Functions الدوال المساعده
// =========================================================

float toRadians(float degree) {
    return degree * PI / 180.0f;
}

void wrapAngle(float& angle) {
    if (angle > 360.0f) {
        angle -= 360.0f;
    }
}

void clampValue(float& value, float minValue, float maxValue) {
    if (value < minValue) value = minValue;
    if (value > maxValue) value = maxValue;
}

void setColor(Color color) {
    glColor3f(color.r, color.g, color.b);
}

// =========================================================
// Texture Functions دوال الانسجه 
// =========================================================

GLuint createFallbackTexture(Color color) {      //انشاء نسيج عند فقدان الصور
    const int size = 64;
    unsigned char data[size][size][3];

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float shade = (((x / 8) + (y / 8)) % 2 == 0) ? 1.0f : 0.72f;

            data[y][x][0] = (unsigned char)(color.r * 255.0f * shade);
            data[y][x][1] = (unsigned char)(color.g * 255.0f * shade);
            data[y][x][2] = (unsigned char)(color.b * 255.0f * shade);
        }
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureId;
}

unsigned int readUInt32(unsigned char* header, int offset) {   // قراءه بيانات الصور وتحليل بياناتها 
    return header[offset] |
        (header[offset + 1] << 8) |
        (header[offset + 2] << 16) |
        (header[offset + 3] << 24);
}

unsigned short readUInt16(unsigned char* header, int offset) {
    return header[offset] | (header[offset + 1] << 8);
}

GLuint LoadTexture(const char* filename, bool& success) {   //يحمل صور الارض والقمر منbmp
    success = false;

    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        cout << "Texture not found: " << filename << endl;
        return 0;
    }

    unsigned char header[54];

    if (fread(header, 1, 54, file) != 54 || header[0] != 'B' || header[1] != 'M') {
        fclose(file);
        return 0;
    }

    unsigned int dataOffset = readUInt32(header, 0x0A);
    unsigned int width = readUInt32(header, 0x12);
    unsigned int height = readUInt32(header, 0x16);
    unsigned short bitsPerPixel = readUInt16(header, 0x1C);

    if (bitsPerPixel != 24) {
        fclose(file);
        return 0;
    }

    unsigned int rowPadded = (width * 3 + 3) & (~3);
    unsigned int imageSize = rowPadded * height;

    unsigned char* rawData = new unsigned char[imageSize];
    unsigned char* imageData = new unsigned char[width * height * 3];

    fseek(file, dataOffset, SEEK_SET);
    fread(rawData, 1, imageSize, file);
    fclose(file);

    for (unsigned int y = 0; y < height; y++) {
        for (unsigned int x = 0; x < width; x++) {
            unsigned int rawIndex = y * rowPadded + x * 3;
            unsigned int imageIndex = ((height - 1 - y) * width + x) * 3;

            imageData[imageIndex] = rawData[rawIndex + 2];
            imageData[imageIndex + 1] = rawData[rawIndex + 1];
            imageData[imageIndex + 2] = rawData[rawIndex];
        }
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, imageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    delete[] rawData;
    delete[] imageData;

    success = true;
    return textureId;
}

// =========================================================
// Text Functions دوال النصوص
// =========================================================

void drawText2D(float x, float y, const char* text, void* font) {   //كتابه النصوص المعلوماتيه ع الشاشه
    glRasterPos2f(x, y);

    for (int i = 0; text[i] != '\0'; i++) {
        glutBitmapCharacter(font, text[i]);
    }
}

void setup2D() {      // ضبط مصفوفه الاسقاط
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}

void restore3D() {   // العوده للرسم الثلاثي الابعاد
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
}

// =========================================================
// Initialization مرحله التهيئه
// =========================================================

void initializeMoonPhases() {  // تعريف اسماء والوان وزوايا اطوار القمر
    phases[0] = { "New Moon", "New Moon: The moon is between Earth and the invisible light source.", 0.0f, {0.70f, 0.70f, 0.75f} };
    phases[1] = { "Waxing Crescent", "Waxing Crescent: A small bright part appears on the right side.", 45.0f, {0.80f, 0.82f, 0.90f} };
    phases[2] = { "First Quarter", "First Quarter: Half of the moon is visible.", 90.0f, {0.85f, 0.85f, 0.90f} };
    phases[3] = { "Waxing Gibbous", "Waxing Gibbous: More than half of the moon is illuminated.", 135.0f, {0.90f, 0.90f, 0.92f} };
    phases[4] = { "Full Moon", "Full Moon: The full face of the moon is visible.", 180.0f, {1.0f, 0.95f, 0.65f} };
    phases[5] = { "Waning Gibbous", "Waning Gibbous: The illuminated part starts decreasing.", 225.0f, {0.90f, 0.90f, 0.92f} };
    phases[6] = { "Last Quarter", "Last Quarter: Half of the moon is visible again.", 270.0f, {0.85f, 0.85f, 0.90f} };
    phases[7] = { "Waning Crescent", "Waning Crescent: A small bright part remains before the cycle repeats.", 315.0f, {0.80f, 0.82f, 0.90f} };
}

void initializeStars() { // توليد مواقع وسطوع للنجوم
    for (int i = 0; i < STAR_COUNT; i++) {
        float angle = i * 12.9898f;
        float radius = 18.0f + (i % 75);

        starX[i] = sin(angle) * radius;
        starY[i] = (((i * 37) % 160) - 80) / 3.0f;
        starZ[i] = cos(angle * 1.7f) * radius;
        starBrightness[i] = 0.50f + (i % 6) * 0.08f;
    }
}

void initializeTextures() { // ربط ملفات الصور بالكائنات الرسوميه
    earthTextureId = LoadTexture("../assets/earth.bmp", earthHasTexture);
    moonTextureId = LoadTexture("../assets/moon.bmp", moonHasTexture);

    if (!earthHasTexture) {
        earthTextureId = createFallbackTexture({ 0.20f, 0.45f, 0.95f });
    }

    if (!moonHasTexture) {
        moonTextureId = createFallbackTexture({ 0.75f, 0.75f, 0.75f });
    }
}

void initializeLighting() { // توليد الاضاءة
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

    GLfloat lightPosition[] = { -12.0f, 4.0f, 6.0f, 1.0f };
    GLfloat lightAmbient[] = { 0.28f, 0.28f, 0.34f, 1.0f };
    GLfloat lightDiffuse[] = { 1.0f, 0.96f, 0.86f, 1.0f };
    GLfloat lightSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
}

void init() { // ضبط الوان الخلفيه والعمق
    glClearColor(0.01f, 0.01f, 0.04f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    sphereQuadric = gluNewQuadric();
    gluQuadricTexture(sphereQuadric, GL_TRUE);
    gluQuadricNormals(sphereQuadric, GLU_SMOOTH);

    initializeMoonPhases();
    initializeStars();
    initializeTextures();
    initializeLighting();
}

// =========================================================
// Drawing Functions دوال الرسم
// =========================================================

void drawStars() { // رسم النجوم في الفضاء الخلفي
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glPointSize(2.0f);
    glBegin(GL_POINTS);

    for (int i = 0; i < STAR_COUNT; i++) {
        float brightness = starBrightness[i];
        glColor3f(brightness, brightness, brightness);
        glVertex3f(starX[i], starY[i], starZ[i]);
    }

    glEnd();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

void drawMoonOrbit() { // رسم خط مدار القمر حول الارض
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glColor3f(0.32f, 0.36f, 0.52f);
    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < 220; i++) {
        float angle = 2.0f * PI * i / 220.0f;
        glVertex3f(cos(angle) * moonOrbitRadius, 0.0f, sin(angle) * moonOrbitRadius);
    }

    glEnd();

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

void drawPhaseMarkers() {
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    for (int i = 0; i < PHASE_COUNT; i++) {
        float angle = toRadians(phases[i].angle);
        float x = cos(angle) * moonOrbitRadius;
        float z = sin(angle) * moonOrbitRadius;

        glPushMatrix();
        glTranslatef(x, 0.0f, z);

        if (i == selectedPhase) {
            glColor3f(1.0f, 0.86f, 0.30f);
            glutSolidSphere(0.22f, 24, 24);
        }
        else {
            setColor(phases[i].color);
            glutSolidSphere(0.13f, 18, 18);
        }

        glPopMatrix();
    }

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
}

void drawEarth() { // رسم كوكب الارض في المركز مع النسيج
    glPushMatrix();

    glRotatef(earthSelfAngle, 0.0f, 1.0f, 0.0f);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, earthTextureId);
    gluSphere(sphereQuadric, 2.1f, 72, 72);

    glPopMatrix();
}

void drawMoon() { // حساب موقع القمر ورسمه في المدار
    float angle = toRadians(moonOrbitAngle);
    float x = cos(angle) * moonOrbitRadius;
    float z = sin(angle) * moonOrbitRadius;

    glPushMatrix();

    glTranslatef(x, 0.0f, z);
    glRotatef(moonSelfAngle, 0.0f, 1.0f, 0.0f);

    if (selectedPhase >= 0) {
        glScalef(1.35f, 1.35f, 1.35f);
    }

    glColor3f(1.0f, 1.0f, 1.0f);
    glBindTexture(GL_TEXTURE_2D, moonTextureId);
    gluSphere(sphereQuadric, 0.85f, 64, 64);

    glPopMatrix();
}

void drawCircle2D(float centerX, float centerY, float radius, bool filled) {
    glBegin(filled ? GL_POLYGON : GL_LINE_LOOP);

    for (int i = 0; i < 360; i++) {
        float angle = 2.0f * PI * i / 360.0f;
        glVertex2f(centerX + cos(angle) * radius, centerY + sin(angle) * radius);
    }

    glEnd();
}

void drawMoonPhasePreview(float centerX, float centerY, float radius, float phaseAngle) { // رسم هندسي لطور القمر
    glColor3f(0.08f, 0.08f, 0.10f);
    drawCircle2D(centerX, centerY, radius, true);

    float alpha = toRadians(phaseAngle);
    float lightX = sin(alpha);
    float lightZ = -cos(alpha);

    glPointSize(2.0f);
    glBegin(GL_POINTS);

    for (float y = -1.0f; y <= 1.0f; y += 0.012f) {
        for (float x = -1.0f; x <= 1.0f; x += 0.012f) {
            float rr = x * x + y * y;

            if (rr <= 1.0f) {
                float z = sqrt(1.0f - rr);
                float dot = x * lightX + z * lightZ;

                if (dot > 0.0f) {
                    glColor3f(0.95f, 0.95f, 0.86f);
                }
                else {
                    glColor3f(0.10f, 0.10f, 0.13f);
                }

                glVertex2f(centerX + x * radius, centerY + y * radius);
            }
        }
    }

    glEnd();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawCircle2D(centerX, centerY, radius, false);
}

void drawHUD() { // رسم واجهه المعلومات والتحكم
    setup2D();

    glColor3f(1.0f, 1.0f, 1.0f);
    drawText2D(24.0f, windowHeight - 34.0f, "Interactive Moon Phases System", GLUT_BITMAP_HELVETICA_18);

    glColor3f(0.78f, 0.84f, 1.0f);
    drawText2D(24.0f, windowHeight - 64.0f, "Mouse: Select Phase | Space: Pause | Arrows: Camera | +/-: Zoom | R: Reset", GLUT_BITMAP_HELVETICA_12);

    if (selectedPhase >= 0) {
        glColor3f(1.0f, 0.88f, 0.35f);
        drawText2D(24.0f, 82.0f, phases[selectedPhase].name, GLUT_BITMAP_HELVETICA_18);

        glColor3f(0.92f, 0.92f, 0.92f);
        drawText2D(24.0f, 54.0f, phases[selectedPhase].info, GLUT_BITMAP_HELVETICA_12);

        drawMoonPhasePreview(windowWidth - 120.0f, 100.0f, 58.0f, phases[selectedPhase].angle);
    }
    else {
        glColor3f(0.92f, 0.92f, 0.92f);
        drawText2D(24.0f, 54.0f, "Click on any moon phase marker to enlarge the Moon and show information.", GLUT_BITMAP_HELVETICA_12);
    }

    restore3D();
}

// =========================================================
// Animation and Selection دوال التحريك والاختيار
// =========================================================

void updateAnimation() { // تحديث زوايا الدوران الذاتي والمداري
    if (!isPaused) {
        earthSelfAngle += 0.35f;
        moonOrbitAngle += 0.12f;
        moonSelfAngle += 0.55f;

        wrapAngle(earthSelfAngle);
        wrapAngle(moonOrbitAngle);
        wrapAngle(moonSelfAngle);

        selectedPhase = (int)((moonOrbitAngle + 22.5f) / 45.0f) % PHASE_COUNT;
    }

    glutPostRedisplay();
}

void getPhaseScreenPosition(MoonPhase phase, double& screenX, double& screenY) { // تحويل من ثري الئ تو دي
    double modelView[16], projection[16];
    int viewport[4];

    glGetDoublev(GL_MODELVIEW_MATRIX, modelView);
    glGetDoublev(GL_PROJECTION_MATRIX, projection);
    glGetIntegerv(GL_VIEWPORT, viewport);

    double angleRad = phase.angle * PI / 180.0;
    double worldX = cos(angleRad) * moonOrbitRadius;
    double worldZ = sin(angleRad) * moonOrbitRadius;
    double winZ;

    gluProject(worldX, 0.0, worldZ, modelView, projection, viewport, &screenX, &screenY, &winZ);

    screenY = windowHeight - screenY;
}

void selectPhaseByMouse(int mouseX, int mouseY) { // تحديث طور القمر بناء ع نقرة الفارة
    double minDistance = 999999.0;
    int nearestPhase = -1;

    for (int i = 0; i < PHASE_COUNT; i++) {
        double sx, sy;
        getPhaseScreenPosition(phases[i], sx, sy);

        double dx = mouseX - sx;
        double dy = mouseY - sy;
        double distance = sqrt(dx * dx + dy * dy);

        if (distance < 35.0 && distance < minDistance) {
            minDistance = distance;
            nearestPhase = i;
        }
    }

    if (nearestPhase >= 0) {
        selectedPhase = nearestPhase;
        moonOrbitAngle = phases[selectedPhase].angle;
        isPaused = true;
    }
}

// =========================================================
// Callback Functions دوال الاستدعاء
// =========================================================

void display() { // الوظيفه المركزيه لمسح الشاشة واعاد الرسم
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(0.0f, 0.0f, -cameraDistance);
    glRotatef(cameraAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(cameraAngleY, 0.0f, 1.0f, 0.0f);

    GLfloat lightPosition[] = { -12.0f, 4.0f, 6.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

    drawStars();
    drawMoonOrbit();
    drawPhaseMarkers();
    drawEarth();
    drawMoon();
    drawHUD();

    glutSwapBuffers();
}

void reshapeFunc(int width, int height) { // تغير ابعاد نافذه العرض
    windowWidth = width;
    windowHeight = height;

    if (height == 0) {
        height = 1;
    }

    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 1.0, 150.0);

    glMatrixMode(GL_MODELVIEW);
}

void mouse(int button, int state, int x, int y) { // نقرات الفاره لاختيار طور القمر
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        selectPhaseByMouse(x, y);
        glutPostRedisplay();
    }
}

void key(unsigned char key, int x, int y) { // مفاتيح الاغلاق والتحكم 
    switch (key) {
    case 27:
        exit(0);

    case ' ':
        isPaused = !isPaused;
        break;

    case '+':
    case '=':
        cameraDistance -= 2.0f;
        clampValue(cameraDistance, 12.0f, 60.0f);
        break;

    case '-':
    case '_':
        cameraDistance += 2.0f;
        clampValue(cameraDistance, 12.0f, 60.0f);
        break;

    case 'r':
    case 'R':
        cameraDistance = 28.0f;
        cameraAngleX = 24.0f;
        cameraAngleY = -32.0f;
        selectedPhase = -1;
        isPaused = false;
        moonOrbitAngle = 0.0f;
        break;
    }

    glutPostRedisplay();
}

void special(int key, int x, int y) { // تدوير الكاميرا باستخدام مفاتيح الاسهم
    switch (key) {
    case GLUT_KEY_LEFT:
        cameraAngleY -= 4.0f;
        break;

    case GLUT_KEY_RIGHT:
        cameraAngleY += 4.0f;
        break;

    case GLUT_KEY_UP:
        cameraAngleX -= 4.0f;
        break;

    case GLUT_KEY_DOWN:
        cameraAngleX += 4.0f;
        break;
    }

    glutPostRedisplay();
}

// =========================================================
// Main Function الدالة الرئيسية
// =========================================================

int main(int argc, char** argv) {
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Interactive Moon Phases System");

#ifndef __APPLE__
    glewInit();
#endif

    init();

    glutDisplayFunc(display);
    glutReshapeFunc(reshapeFunc);
    glutMouseFunc(mouse);
    glutKeyboardFunc(key);
    glutSpecialFunc(special);
    glutIdleFunc(updateAnimation);

    glutMainLoop();

    return 0;
}