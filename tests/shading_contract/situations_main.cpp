/**
 * tests/shading_contract/situations_main.cpp
 *
 * The entry point of test_shading_contract_situations (spec 026, T044): ShadingContract_Situations in
 * an executable of its own, EXCLUDE_FROM_ALL until Phase 9, because it reads report fields that only
 * Phase 9 adds (CPointKeyReport::dim and friends) and would otherwise stop the whole rendered program
 * from compiling in Phases 3 to 8. With no argument it runs every scene (each in a child process); the
 * hidden "scene <name>" form renders one.
 */
int caseSituations(int argc, char **argv);

int main(int argc, char **argv) {
    return caseSituations(argc, argv);
}
