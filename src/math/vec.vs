type Vec3: struct {
    x: float64;
    y: float64;
    z: float64;
}

impl Vec3 {
    fn lsqr(use v: Vec3) -> float64 {
        return x * x + y * y + z * z;
    }
    fn add(use v: Vec3, o: Vec3) -> Vec3 {
        return Vec3::{x + o.x, y + o.y, z + o.z};
    }
    fn scale(use v: Vec3, f: float64) -> Vec3 {
        return Vec3::{x * f, y * f, z * f};
    }
    fn dot(use v: Vec3, o: Vec3) -> float64 {
        return x * o.x + y * o.y + z * o.z;
    }
    fn cross(use v: Vec3, o: Vec3) -> Vec3 {
        return Vec3::{y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
}

type Mat4: [4][4]float64;
