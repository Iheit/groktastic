#define main groktastic_internal_main
#include "main.cpp"
#undef main

int main() {
    return groktastic_internal_main();
}
