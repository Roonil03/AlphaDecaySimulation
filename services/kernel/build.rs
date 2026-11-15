fn main() {
    cc::Build::new().cpp(true).file("native/wkb.cpp").flag_if_supported("-std=c++11").compile("wkb");
    cc::Build::new().file("native/potential.c").compile("potential");
    println!("cargo:rerun-if-changed=native/potential.c");
    println!("cargo:rerun-if-changed=native/wkb.cpp");
}
