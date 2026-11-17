import json, subprocess, sys
exe=sys.argv[1]
base={"schema_version":1,"parent_z":52,"parent_a":104,"q_mev":5.23,"l":0,"radius_fm":9,"preformation":.1,"assault_hz":1e21,"potential_model":"woods_saxon","nuclear_radius_fm":9,"charge_radius_fm":9}
def run(x):
    return subprocess.run([exe,"--run"], input=json.dumps(x), text=True, capture_output=True)
request=dict(base,numerov={"lead_radius_fm":5,"match_radius_fm":500,"step_fm":.05},uncertainty={"sample_count":20,"seed":3,"standard_deviations":[.09,0,0,0]})
r=run(request); assert r.returncode==0,r.stderr
x=json.loads(r.stdout); assert x['status']=='ok' and x['numerov']['status']=='converged' and x['uncertainty']['accepted']==20
assert x['uncertainty']['assumed_independent'] and x['numerov']['step_error_decades']<=1e-4
assert json.loads(run(request).stdout)['uncertainty']==x['uncertainty']
for key,value in [('seed',1.0),('seed',-1),('sample_count',1.5),('sample_count',1000001)]:
    r=run(dict(base,uncertainty={key:value})); assert r.returncode!=0,(key,value,r.stdout)
r=run(dict(base,uncertainty={'covariance':[[1,0,0,0],[0,-1,0,0],[0,0,0,0],[0,0,0,0]]})); assert r.returncode!=0
r=run(dict(base,numerov={'lead_radius_fm':-1})); assert r.returncode==0 and json.loads(r.stdout)['numerov']['status']=='invalid_input'
