

/*

#include <Arduino.h>
#include <unity.h>
#include "../../src/modulos/simuladorCurvas/simuladorCurvas.h" 
#include "../../src/modulos/carga_electronica/carga_electronica.h"
#include "../../src/modulos/serialCom/serialCom.h"
// Test: inicializar array y crear curva
void test_create_curve(void) {
    simuladorCurvasInit(3);   // inicializo espacio para 3 curvas
    TEST_ASSERT_EQUAL(0, getCurveCount());  // debería estar vacío

    int id = createCurve(5);  // creo curva en pin 5
    TEST_ASSERT_TRUE(id >= 0);   // id válido
    TEST_ASSERT_EQUAL(1, getCurveCount());  // ahora debería haber 1 curva
}

void setup() {
    delay(2000);        // darle tiempo al serial monitor
    UNITY_BEGIN();      // arranca framework de testing
    RUN_TEST(test_create_curve); // corre el test
    UNITY_END();        // termina
}

void loop() {
    // vacío
}
*/