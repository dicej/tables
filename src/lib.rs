#![deny(warnings)]

use {
    libc::size_t,
    std::ffi::{c_int, c_void},
};

#[allow(dead_code)]
#[repr(C)]
struct Table {
    entries: *mut c_void,
    mask: size_t,
    used: size_t,
}

#[allow(dead_code)]
extern "C" {
    fn socket_table_insert(value: c_int, fd: c_int, table: *mut Table) -> bool;
    fn socket_table_get(fd: c_int, value: *mut c_int, table: *const Table) -> bool;
    fn socket_table_remove(fd: c_int, value: *mut c_int, table: *mut Table) -> bool;
    fn socket_table_next(
        index: *mut size_t,
        key: *mut c_int,
        value: *mut c_int,
        table: *const Table,
    ) -> bool;
    fn socket_table_free(table: *mut Table);
}

#[cfg(test)]
mod tests {
    use {
        super::*,
        proptest::{
            strategy::Strategy,
            test_runner::{Config, TestRng, TestRunner},
        },
        std::{collections::HashMap, error::Error, ptr},
    };

    const MAX_SIZE: usize = 1000;

    #[derive(Debug)]
    enum Action {
        Insert { key: c_int, value: c_int },
        Remove(usize),
    }

    fn assert_equal(map: &HashMap<c_int, c_int>, table: &Table) {
        for (key, value) in map {
            let mut table_value = 0;
            assert!(unsafe { socket_table_get(*key, &mut table_value, table) });
            assert_eq!(*value, table_value);
        }

        let mut index = 0;
        loop {
            let mut key = 0;
            let mut table_value = 0;
            if unsafe { socket_table_next(&mut index, &mut key, &mut table_value, table) } {
                assert_eq!(Some(table_value), map.get(&key).copied());
            } else {
                break;
            }
        }
    }

    #[test]
    fn test() -> Result<(), Box<dyn Error>> {
        let config = Config::default();
        let mut runner = if true {
            TestRunner::new(config)
        } else {
            let seed = [0u8; 32];
            let algorithm = config.rng_algorithm;
            TestRunner::new_with_rng(config, TestRng::from_seed(algorithm, &seed))
        };

        let strategy = proptest::collection::vec(
            (0..5).prop_flat_map(|index| match index {
                0 => proptest::num::usize::ANY.prop_map(Action::Remove).boxed(),
                _ => (proptest::num::i32::ANY, proptest::num::i32::ANY)
                    .prop_map(|(key, value)| Action::Insert { key, value })
                    .boxed(),
            }),
            0..MAX_SIZE,
        );

        runner.run(&strategy, |actions| {
            if false {
                print!(".");
                use std::io::Write;
                _ = std::io::stdout().flush();
            }

            let mut keys = Vec::with_capacity(actions.len());

            let mut table = Table {
                entries: ptr::null_mut(),
                mask: 0,
                used: 0,
            };

            let mut map = HashMap::new();

            for action in actions {
                match action {
                    Action::Insert { key, value } => {
                        keys.push(key);
                        assert!(unsafe { socket_table_insert(value, key, &mut table) });
                        map.insert(key, value);
                    }
                    Action::Remove(index) => {
                        if !keys.is_empty() {
                            let key = keys[index % keys.len()];
                            let value = map.remove(&key);
                            let mut table_value = 0;
                            if unsafe { socket_table_remove(key, &mut table_value, &mut table) } {
                                assert_eq!(value, Some(table_value));
                            } else {
                                assert!(value.is_none());
                            }
                        }
                    }
                }
            }

            assert_equal(&map, &table);

            for key in keys {
                let value = map.remove(&key);
                let mut table_value = 0;
                if unsafe { socket_table_remove(key, &mut table_value, &mut table) } {
                    assert_eq!(value, Some(table_value));
                } else {
                    assert!(value.is_none());
                }
            }

            assert_equal(&map, &table);

            unsafe { socket_table_free(&mut table) };

            Ok(())
        })?;

        Ok(())
    }
}
