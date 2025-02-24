#include <iostream>
#include <string>
#define GLM_ENAGLE_EXPERIMENTAL
#include "glm/gtx/matrix"
#include "glm/gtx/string_cast.hpp"

int main(int argc, char **argv) {

    // From A to B
    glm::vec3 AtoB = B - A;
    cout << "A: " << glm::to_string(A) << endl;
    cout << "B: " << glm::to_string(B) << endl;
    cout << "AtoB: " << glm::to_string(AtoB) << endl;

    return 0;
}