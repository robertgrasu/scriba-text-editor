use std::collections::HashMap;

/// docs
#[derive(Debug)]
struct P<'a> { name: &'a str, n: u32 }

fn main() {
    let mut m: HashMap<String, i32> = HashMap::new();
    m.insert("a".to_string(), 1);
    println!("{:?} {}", m, 3.5);
}
