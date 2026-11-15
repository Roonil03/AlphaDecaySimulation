use axum::{http::StatusCode, routing::{get, post}, Json, Router};
use serde::{Deserialize, Serialize};

const HBAR_C: f64 = 197.3269804; // MeV fm
const AMU_C2: f64 = 931.49410242; // MeV
const E2: f64 = 1.43996448; // MeV fm

unsafe extern "C" {
    #[cfg(test)]
    fn barrier_action_refined(radius: f64, outer: f64, q: f64, coulomb: f64, centrifugal: f64, mu: f64) -> f64;
    #[cfg(test)]
    fn barrier_action_portable(radius: f64, outer: f64, q: f64, coulomb: f64, centrifugal: f64, mu: f64) -> f64;
    fn barrier_action(radius: f64, outer: f64, q: f64, coulomb: f64, centrifugal: f64, mu: f64) -> f64;
}

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct Request {
    parent_z: i32,
    parent_a: i32,
    q_mev: f64,
    l: i32,
    radius_fm: f64,
    preformation: f64,
    assault_hz: f64,
    #[serde(default)]
    research_model: bool,
    #[serde(default)]
    parent_binding_mev: Option<f64>,
}

#[derive(Debug, Serialize)]
struct Response {
    log10_half_life_s: f64,
    log10_penetrability: f64,
    outer_turning_fm: f64,
    #[serde(skip_serializing_if = "Option::is_none")]
    research_log10_half_life_s: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    shell_log10_half_life_s: Option<f64>,
}

// Qi et al. (2009), alpha-only fit, Eq. 8.
fn universal_decay_law(p: &Request) -> f64 {
    let daughter_a = (p.parent_a - 4) as f64;
    let reduced_a = 4.0 * daughter_a / p.parent_a as f64;
    let charge_product = 2.0 * (p.parent_z - 2) as f64;
    let chi = charge_product * (reduced_a / p.q_mev).sqrt();
    let rho = (reduced_a * charge_product * (daughter_a.cbrt() + 4.0_f64.cbrt())).sqrt();
    0.4065 * chi - 0.4311 * rho - 20.7889
}

// Ren et al. (2026), Eqs. 2–4, specialized to even-even l=0.
fn shell_corrected_royer(p: &Request, binding: f64) -> f64 {
    let a = p.parent_a as f64;
    let z = p.parent_z as f64;
    let liquid_drop = 15.5287 * a - 16.9043 * a.powf(2.0 / 3.0)
        - 91.9686 * (a / 2.0 - z).powi(2) / a
        - 0.7025 * z * z / a.cbrt() + 12.4439 / a.sqrt();
    let shell = binding - liquid_drop;
    -28.1919 - 1.0853 * a.powf(1.0 / 6.0) * z.sqrt()
        + 1.6260 * z / p.q_mev.sqrt() + 0.1078 * (shell - 2.0)
}

fn calculate(p: &Request) -> Result<Response, &'static str> {
    if !(3..=130).contains(&p.parent_z) || !(5..=400).contains(&p.parent_a)
        || p.parent_a - p.parent_z < 2 || !(0..=100).contains(&p.l)
        || !p.q_mev.is_finite() || !(0.01..=100.0).contains(&p.q_mev)
        || !p.radius_fm.is_finite() || !(0.1..=100.0).contains(&p.radius_fm)
        || !p.preformation.is_finite() || !(0.0 < p.preformation && p.preformation <= 1.0)
        || !p.assault_hz.is_finite() || !(1e-300..=1e30).contains(&p.assault_hz)
        || p.parent_binding_mev.is_some_and(|v| !v.is_finite() || !(0.0 < v && v <= 10000.0)) {
        return Err("input outside supported domain; see kernel README");
    }
    let daughter_a = (p.parent_a - 4) as f64;
    let mu = AMU_C2 * 4.0 * daughter_a / (4.0 + daughter_a);
    let c = 2.0 * (p.parent_z - 2) as f64 * E2;
    let angular = p.l as f64 * (p.l + 1) as f64;
    let d = HBAR_C * HBAR_C * angular / (2.0 * mu);
    let outer = (c + (c * c + 4.0 * p.q_mev * d).sqrt()) / (2.0 * p.q_mev);
    let action = unsafe { barrier_action(p.radius_fm, outer, p.q_mev, c, d, mu) };
    let log_p = -2.0 * action / std::f64::consts::LN_10;
    let log_t = std::f64::consts::LN_2.log10() - p.preformation.log10() - p.assault_hz.log10() - log_p;
    let eligible = p.parent_z % 2 == 0 && (p.parent_a - p.parent_z) % 2 == 0 && p.l == 0;
    let research = (p.research_model && eligible).then(|| universal_decay_law(p));
    let shell = if p.research_model && eligible { p.parent_binding_mev.map(|v| shell_corrected_royer(p, v)) } else { None };
    Ok(Response { log10_half_life_s: log_t, log10_penetrability: log_p, outer_turning_fm: outer, research_log10_half_life_s: research, shell_log10_half_life_s: shell })
}

async fn barrier(Json(p): Json<Request>) -> Result<Json<Response>, (StatusCode, &'static str)> {
    calculate(&p).map(Json).map_err(|e| (StatusCode::UNPROCESSABLE_ENTITY, e))
}
fn app() -> Router { Router::new().route("/health", get(|| async { "ok" })).route("/v1/barrier", post(barrier)) }

#[tokio::main]
async fn main() {
    let listener = tokio::net::TcpListener::bind("0.0.0.0:8081").await.expect("bind port 8081");
    axum::serve(listener, app()).await.expect("serve HTTP");
}

#[cfg(test)]
mod tests {
    use super::*;
    use axum::{body::Body, http::Request as HttpRequest};
    use http_body_util::BodyExt;
    use tower::ServiceExt;

    fn example() -> Request { Request { parent_z: 92, parent_a: 238, q_mev: 4.27, l: 0, radius_fm: 9.0, preformation: 0.1, assault_hz: 1e21, research_model: false, parent_binding_mev: None } }
    #[test]
    fn s_wave_matches_analytic_integral() {
        let p = example(); let out = calculate(&p).unwrap();
        let mu = AMU_C2 * 4.0 * (p.parent_a - 4) as f64 / p.parent_a as f64;
        let x = p.radius_fm / out.outer_turning_fm;
        let action = (2.0 * mu * p.q_mev).sqrt() * out.outer_turning_fm / HBAR_C
            * (x.sqrt().acos() - (x * (1.0 - x)).sqrt());
        assert!((out.log10_penetrability + 2.0 * action / std::f64::consts::LN_10).abs() < 1e-7);
    }
    #[test]
    fn portable_matches_assembly() {
        for l in [0.0, 1.0, 10.0, 100.0] {
            let mu = AMU_C2 * 4.0 * 234.0 / 238.0;
            let c = 180.0 * E2;
            let d = HBAR_C * HBAR_C * l * (l + 1.0) / (2.0 * mu);
            let q = 4.27;
            let b = (c + (c * c + 4.0 * q * d).sqrt()) / (2.0 * q);
            let (a, p) = unsafe { (barrier_action(9.0, b, q, c, d, mu), barrier_action_portable(9.0, b, q, c, d, mu)) };
            assert!((a - p).abs() < 1e-10);
        }
    }
    #[test]
    #[ignore]
    fn benchmark() {
        let p = example(); let iterations = 10000;
        let start = std::time::Instant::now();
        for _ in 0..iterations { std::hint::black_box(calculate(std::hint::black_box(&p)).unwrap()); }
        eprintln!("iterations={iterations} elapsed_ms={} ns_per_call={}", start.elapsed().as_millis(), start.elapsed().as_nanos() / iterations);
    }
    #[test]
    fn extreme_quadrature_converges() {
        let p = Request { parent_z: 130, parent_a: 400, q_mev: 0.01, l: 0, radius_fm: 0.1, preformation: 1.0, assault_hz: 1e21, research_model: false, parent_binding_mev: None };
        let mu = AMU_C2 * 4.0 * 396.0 / 400.0;
        let c = 256.0 * E2;
        let b = c / p.q_mev;
        let x = p.radius_fm / b;
        let expected = (2.0 * mu * p.q_mev).sqrt() * b / HBAR_C * (x.sqrt().acos() - (x * (1.0 - x)).sqrt());
        let actual = unsafe { barrier_action(p.radius_fm, b, p.q_mev, c, 0.0, mu) };
        assert!((actual - expected).abs() < 1e-6, "{actual} {expected}");
        let d = HBAR_C * HBAR_C * 100.0 * 101.0 / (2.0 * mu);
        let outer = (c + (c*c + 4.0*p.q_mev*d).sqrt()) / (2.0*p.q_mev);
        let a = unsafe { barrier_action(p.radius_fm, outer, p.q_mev, c, d, mu) };
        let b = unsafe { barrier_action_refined(p.radius_fm, outer, p.q_mev, c, d, mu) };
        assert!((a-b).abs() < 1e-5, "{a} {b}");
    }
    #[test]
    fn physical_edges_and_validation() {
        let mut p = example(); let s = calculate(&p).unwrap();
        p.l = 5; assert!(calculate(&p).unwrap().log10_penetrability < s.log10_penetrability);
        p.radius_fm = 100.0; let no_barrier = calculate(&p).unwrap();
        assert_eq!(no_barrier.log10_penetrability, 0.0);
        p.preformation = 0.0; assert!(calculate(&p).is_err());
        p.preformation = 1.0; p.q_mev = f64::NAN; assert!(calculate(&p).is_err());
        p.q_mev = 4.0; p.parent_a = 5; p.parent_z = 10; assert!(calculate(&p).is_err());
    }
    #[test]
    fn research_comparison_scope_and_independence() {
        let mut p = example();
        let classic = calculate(&p).unwrap();
        assert!(classic.research_log10_half_life_s.is_none());
        p.research_model = true;
        let researched = calculate(&p).unwrap();
        assert!((researched.research_log10_half_life_s.unwrap() - 17.501939426540606).abs() < 1e-10);
        assert_eq!(classic.log10_half_life_s, researched.log10_half_life_s);
        p.radius_fm = 8.0; p.preformation = 0.5; p.assault_hz = 1e20;
        assert_eq!(researched.research_log10_half_life_s, calculate(&p).unwrap().research_log10_half_life_s);
        p.parent_z = 84; p.parent_a = 212; p.q_mev = 8.95420;
        assert!((calculate(&p).unwrap().research_log10_half_life_s.unwrap() + 6.630368823101762).abs() < 1e-10);
        assert!(calculate(&p).unwrap().shell_log10_half_life_s.is_none());
        p.parent_binding_mev = Some(1655.7716856);
        let shell = calculate(&p).unwrap().shell_log10_half_life_s.unwrap();
        assert!((shell + 6.500350233085122).abs() < 1e-10);
        p.parent_binding_mev = Some(1656.7716856);
        assert!((calculate(&p).unwrap().shell_log10_half_life_s.unwrap() - shell - 0.1078).abs() < 1e-10);
        p.parent_z = 88; p.parent_a = 226; p.q_mev = 4.871;
        assert!((calculate(&p).unwrap().research_log10_half_life_s.unwrap() - 11.025033791083192).abs() < 1e-10);
        p.l = 1; assert!(calculate(&p).unwrap().research_log10_half_life_s.is_none());
        assert!(calculate(&p).unwrap().shell_log10_half_life_s.is_none());
        p.l = 0; p.parent_a = 225; assert!(calculate(&p).unwrap().research_log10_half_life_s.is_none());
        p.parent_z = 89; p.parent_a = 226; assert!(calculate(&p).unwrap().research_log10_half_life_s.is_none());
        p.parent_binding_mev = Some(f64::NAN); assert!(calculate(&p).is_err());
    }
    #[tokio::test]
    async fn http_contract() {
        let health = app().oneshot(HttpRequest::builder().uri("/health").body(Body::empty()).unwrap()).await.unwrap();
        assert_eq!(health.status(), StatusCode::OK);
        let body = r#"{"parent_z":92,"parent_a":238,"q_mev":4.27,"l":0,"radius_fm":9.0,"preformation":0.1,"assault_hz":1e21}"#;
        let response = app().oneshot(HttpRequest::builder().method("POST").uri("/v1/barrier").header("content-type", "application/json").body(Body::from(body)).unwrap()).await.unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        let bytes = response.into_body().collect().await.unwrap().to_bytes();
        let json: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
        assert!(json["log10_half_life_s"].as_f64().unwrap().is_finite());
        assert!(json.get("research_log10_half_life_s").is_none());
        assert!(json.get("shell_log10_half_life_s").is_none());
        let body = r#"{"parent_z":92,"parent_a":238,"q_mev":4.27,"l":0,"radius_fm":9.0,"preformation":0.1,"assault_hz":1e21,"research_model":true}"#;
        let response = app().oneshot(HttpRequest::builder().method("POST").uri("/v1/barrier").header("content-type", "application/json").body(Body::from(body)).unwrap()).await.unwrap();
        let bytes = response.into_body().collect().await.unwrap().to_bytes();
        let json: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
        assert!(json["research_log10_half_life_s"].as_f64().unwrap().is_finite());
        assert!(json.get("shell_log10_half_life_s").is_none());
        let body = r#"{"parent_z":84,"parent_a":212,"q_mev":8.9542,"l":0,"radius_fm":9.0,"preformation":0.1,"assault_hz":1e21,"research_model":true,"parent_binding_mev":1655.7716856}"#;
        let response = app().oneshot(HttpRequest::builder().method("POST").uri("/v1/barrier").header("content-type", "application/json").body(Body::from(body)).unwrap()).await.unwrap();
        let bytes = response.into_body().collect().await.unwrap().to_bytes();
        let json: serde_json::Value = serde_json::from_slice(&bytes).unwrap();
        assert!((json["shell_log10_half_life_s"].as_f64().unwrap() + 6.500350233085122).abs() < 1e-10);
        let bad = app().oneshot(HttpRequest::builder().method("POST").uri("/v1/barrier").header("content-type", "application/json").body(Body::from("{}")).unwrap()).await.unwrap();
        assert_eq!(bad.status(), StatusCode::UNPROCESSABLE_ENTITY);
    }
}


