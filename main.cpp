#define NOMINMAX  // Prevent conflicts with Windows' min and max macros

#include <iostream>
#include <cmath>
#include <string>
#include <conio.h>  // For _getch() and _kbhit()
#include <windows.h> // For Sleep()
#include <vector>
#include <omp.h>
#include <chrono>

struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    float dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 normalize() const { float mag = std::sqrt(x * x + y * y + z * z); return Vec3(x / mag, y / mag, z / mag); }
};

struct Ray {
    Vec3 origin, direction;
    Ray(const Vec3& origin, const Vec3& direction) : origin(origin), direction(direction.normalize()) {}
};

struct Sphere {
    Vec3 center;
    float radius;
    Sphere(const Vec3& center, float radius) : center(center), radius(radius) {}

    bool intersect(const Ray& ray, float& t, Vec3& hitPoint, Vec3& normal) const {
        Vec3 oc = ray.origin - center;
        float a = ray.direction.dot(ray.direction);
        float b = 2.0f * oc.dot(ray.direction);
        float c = oc.dot(oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant < 0) return false;  // No intersection

        t = (-b - std::sqrt(discriminant)) / (2.0f * a);
        if (t <= 0) return false;  // Ignore if it's behind the ray origin

        hitPoint = ray.origin + ray.direction * t;
        normal = (hitPoint - center).normalize();
        return true;
    }
};

inline bool isCheckerboard(const Vec3& point) {
    int checkerX = static_cast<int>(std::floor(point.x));
    int checkerZ = static_cast<int>(std::floor(point.z));
    return (checkerX + checkerZ) % 2 == 0;
}

inline char getShade(float t, bool reflective) {
    const std::string shades = " .:-=+*";
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    int index = std::min(static_cast<int>(t * (shades.size() - 1)), static_cast<int>(shades.size() - 1));
    return reflective ? shades[index] : (isCheckerboard(Vec3(t, 0, 0)) ? '*' : ' ');
}

FILE* logFile = nullptr;

void render(int width, int height, const Vec3& camera, std::vector<char>& buffer, std::vector<bool>& changed) {
    Sphere sphere(Vec3(0, 2, 3), 1.0f);  // Sphere positioned at (0, 1, 3)

    float targetAspectRatio = 16.0f / 9.0f;
    float aspectRatio = static_cast<float>(width) / height;

    int adjustedWidth = width;
    int adjustedHeight = static_cast<int>(width / targetAspectRatio);
    if (adjustedHeight > height) {
        adjustedHeight = height;
        adjustedWidth = static_cast<int>(height * targetAspectRatio);
    }

    // Clear changed flags
    std::fill(changed.begin(), changed.end(), false);

    //printf("size: %d x %d\n", adjustedWidth, adjustedHeight);
    

    for (int y = 0; y < adjustedHeight; y++) {
        for (int x = 0; x < adjustedWidth; x++) {
            //printf("This is from thread: %d, %d, %d\n", omp_get_thread_num(), x, y);
            float u = (x - adjustedWidth / 2.0f) / adjustedWidth * aspectRatio;
            float v = (adjustedHeight / 2.0f - y) / adjustedHeight;

            Ray ray(camera, Vec3(u, v, 1));

            float t;
            Vec3 hitPoint, normal;
            char newChar = ' ';

            if (sphere.intersect(ray, t, hitPoint, normal)) {
                Vec3 reflectionDir = ray.direction - normal * 2 * ray.direction.dot(normal);
                Ray reflectionRay(hitPoint, reflectionDir);

                float floorDist = -hitPoint.y / reflectionRay.direction.y;
                if (floorDist > 0) {
                    Vec3 floorPoint = hitPoint + reflectionRay.direction * floorDist;
                    newChar = getShade(1, isCheckerboard(floorPoint));
                }
                else {
                    newChar = getShade(t, true);
                }
            }
            else {
                if (ray.direction.y < 0) {
                    float floorDist = -camera.y / ray.direction.y;
                    Vec3 floorPoint = camera + ray.direction * floorDist;
                    newChar = getShade(1, isCheckerboard(floorPoint));
                }
            }

            // Update buffer only if the character has changed
            if (newChar != buffer[y * width + x]) {
                buffer[y * width + x] = newChar;
                changed[y * width + x] = true; // Mark this position as changed
            }
        }
    }

    //printf("%.2f ms\n", duration / 1000.f);
    //fprintf(logFile, "%.2f ms\n", duration / 1000.f);
    //fflush(logFile);
}

void displayBuffer(int width, int height, const std::vector<char>& buffer, const std::vector<bool>& changed) {
    // Only update changed characters
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (changed[y * width + x]) {
                // Move the cursor to the specific position
                //std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H" << buffer[y * width + x];
                buffer[y * width + x] == '*' ? printf("\033[%d;%dH*", y + 1, x + 1) : printf("\033[%d;%dH ", y + 1, x + 1);
            }
        }
    }
}


int main() {
    int width = 200;   // Terminal width
    int height = 250;  // Terminal height

    Vec3 camera(0, 1, -6);  // Camera position
    std::vector<char> buffer(width * height); // Output buffer
    std::vector<bool> changed(width * height); // Change tracking buffer

    // Clear the screen and set up initial positions
    std::cout << "\033[2J"; // Clear the screen
    std::cout << "\033[H";  // Move cursor to the top left corner

    printf("Press any key to start");

    while (true) {

        char ch = _getch();
        
        switch (ch) {
        case 'w': camera.z += 0.2; break; // Move forward
        case 's': camera.z -= 0.2; break; // Move backward
        case 'a': camera.x -= 0.2; break; // Move left
        case 'd': camera.x += 0.2; break; // Move right
        case 'y': camera.y += 0.2; break; // Move up
        case 'x': camera.y -= 0.2; break; // Move down
        }

        render(width, height, camera, buffer, changed); // Render the scene
        displayBuffer(width, height, buffer, changed); // Display the updated buffer
       
    }

    return 0;
}
