import pickle
import numpy as np

import pysixtrack
import sixtracktools

# Load machine
with open('line.pkl', 'rb') as fid:
    line = pysixtrack.Line.from_dict(pickle.load(fid))

# Load particle on CO
with open('particle_on_CO.pkl', 'rb') as fid:
    part_on_CO = pysixtrack.Particles.from_dict(
        pickle._load(fid))

# Load iconv
with open('iconv.pkl', 'rb') as fid:
    iconv = pickle.load(fid)

# Load sixtrack tracking data
sixdump_all = sixtracktools.SixDump101('res/dump3.dat')
# Assume first particle to be on the closed orbit
Nele_st = len(iconv)
sixdump_CO = sixdump_all[::2][:Nele_st]

# Compute closed orbit using tracking
closed_orbit = line.track_elem_by_elem(part_on_CO)


# Check that closed orbit is closed
pstart = closed_orbit[0].copy()
pstart_st = pysixtrack.Particles(**sixdump_CO[0].get_minimal_beam())

print('STsigma, Sigma, Stdelta, delta, Stpx, px')
for iturn in range(10):
    line.track(pstart)
    line.track(pstart_st)
    print(
        '%e, %e, %e, %e, %e, %e' %
        (pstart_st.sigma,
         pstart.sigma,
         pstart_st.delta,
         pstart.delta,
         pstart_st.px,
         pstart.px))

# Compare closed orbit against sixtrack
for att in 'x px y py delta sigma'.split():
    att_CO = np.array([getattr(pp, att) for pp in closed_orbit])
    att_CO_at_st_ele = att_CO[iconv]
    print('Max C.O. discrepancy in %s %.2e' %
          (att, np.max(np.abs(att_CO_at_st_ele - getattr(sixdump_CO, att)))))


# Compare tracking results
sixdump = sixdump_all[1::2]  # Particle with deviation from CO
# sixdump = sixdump_all[::2] # Particle on CO

p_in_st = pysixtrack.Particles(**sixdump[0].get_minimal_beam())
p_out_st = pysixtrack.Particles(**sixdump[1].get_minimal_beam())

p_in_pyst = p_in_st.copy()
p_out_pyst = p_in_pyst.copy()


for att in 'x px y py delta sigma'.split():
    attin = getattr(p_in_st, att)
    attout = getattr(p_out_st, att)
    print('SxTr: Change in ' + att + ': %e' % (attout - attin))

    attin_pyst = getattr(p_in_pyst, att)
    attout_pyst = getattr(p_out_pyst, att)
    print('PyST: Change in ' + att + ': %e' % (attout_pyst - attin_pyst))


def compare(prun, pbench, pbench_prev):
    out = []
    out_rel = []
    error = False
    for att in 'x px y py delta sigma'.split():
        vrun = getattr(prun, att)
        vbench = getattr(pbench, att)
        vbench_prev = getattr(pbench_prev, att)
        diff = vrun - vbench
        diffrel = abs(1. - abs(vrun - vbench_prev) / abs(vbench - vbench_prev))
        out.append(abs(diff))
        out_rel.append(diffrel)
        print(f"{att:<5} {vrun:22.13e} {vbench:22.13e} {diff:22.13g} {diffrel:22.13g}")
        if diffrel > 1e-8 or np.isnan(diffrel):
            if abs(diff) > 1e-11:
                print('Too large discrepancy!')
                error = True
    print(f"\nmax {max(out):21.12e} maxrel {max(out_rel):22.12e}")
    return error


print("")
for ii in range(1, len(iconv)):
    jja = iconv[ii - 1]
    jjb = iconv[ii]
    prun = pysixtrack.Particles(**sixdump[ii - 1].get_minimal_beam())
    pbench_prev = prun.copy()
    print(f"\n-----sixtrack={ii} sixtracklib={jja} --------------")
    # print(f"pysixtr {jja}, x={prun.x}, px={prun.px}")
    for jj in range(jja + 1, jjb + 1):
        label = line.element_names[jj]
        elem = line.elements[jj]
        pin = prun.copy()
        elem.track(prun)
        print(f"{jj} {label},{str(elem)[:50]}")
    pbench = pysixtrack.Particles(**sixdump[ii].get_minimal_beam())
    # print(f"sixdump {ii}, x={pbench.x}, px={pbench.px}")
    print("-----------------------")
    error = compare(prun, pbench, pbench_prev)
    print("-----------------------\n\n")

    if error:
        print('Error detected')
        break
