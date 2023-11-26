fn main() {
    println!("cargo:rerun-if-changed=src/socket_table.c");

    cc::Build::new()
        .file("src/socket_table.c")
        .compile("socket_table");
}
