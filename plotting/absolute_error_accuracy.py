import glob
import matplotlib.pyplot as plt
import seaborn as sns
from scipy.special import zeta
import numpy as np
from plotters import parse_accuracy_histogram, absolute_error, absolute_relative_error, fancy_names, names, saveplots_flag, showplots_flag
zipf = [1.25, 2.25]
u = 64
m = 1000
streamlen = 30000000

leg = False
fig, (axx1, axx2) = plt.subplots(1, 2)
for z in zipf:
    if (z == 1.25):
        ax2 = plt.axes([0.25, 0.35, .16, .12])
        ax2.set_title('zoom')
        #ax2.set_xlim([1, 70])
        #ax2.set_ylim([1, 2000])
        axx = axx1
        leg = True
    elif (z == 2.25):
        ax2 = plt.axes([0.75, 0.35, .16, .12])
        ax2.set_title('zoom')
        leg = False
        #ax2.set_xlim([1, 30])
        #ax2.set_ylim([1, 200])
        axx = axx2
    else:
        pass
        #ax2.set_xlim([1, 20])
        #ax2.set_ylim([1, 100])

    ax2.set_xlabel('')
    ax2.set_ylabel('')
    axx.indicate_inset_zoom(ax2, edgecolor="black")
    for j, name in enumerate(names):
        n=name.split(' ')
        globname="logs/var_skew*" + n[0] + "_" + n[1] + "*_accuracy_*_" + str(z)+"_*"+str(m)+"_"+str(u)+"_"+str(streamlen)+"_varN_histogram.log"
        #globname="logs/topk_results*"
        fname = glob.glob(globname)[0]
        res, N, PHI = parse_accuracy_histogram(fname)
        error_list = absolute_error(res)
        #error_list = absolute_relative_error(res)
        if error_list == []:
            continue
        ys = list(zip(*error_list))[1]
        xs = list(range(1, len(ys)+1))
        if j == 0:
            leg = True
        else:
            leg = False
        sns.lineplot(xs, ys, label=fancy_names[j], ax=axx, legend=False)
        sns.lineplot(xs, ys, label=fancy_names[j], ax=ax2, legend=False)
    #theoretical_max_err=list(map(lambda x: (m*24 / (30000000)),xs)) #relative error
    theoretical_max_err=list(map(lambda x: (m*24 / (zeta(z)*x**z)),xs)) #absolute error
    avg=list(map(lambda x: np.mean(ys),xs))
    sns.lineplot(xs, (theoretical_max_err),
                 label=r"$\frac{df_sT}{r(e)^a}$", linestyle='dashed', ax=axx, color="r", legend=False)
    sns.lineplot(xs, (theoretical_max_err),
                 label=r"$\frac{df_sT}{r(e)^a}$", linestyle='dashed', ax=ax2, color="r", legend=False)
    #sns.lineplot(xs, (avg),
    #             label="mean", linestyle='dashed', ax=axx, color="g", legend=False)
    #sns.lineplot(xs, (avg),
    #             label="mean", linestyle='dashed', ax=ax2, color="g", legend=False)

    try:  # remove legend and make custom later..
        axx.get_legend().remove()
    except:
        pass

    axx.set_xlabel("Element rank")
    axx.set_ylabel("Absolute error")

handles, labels = axx.get_legend_handles_labels()
# Increased label fontsize.
fig.legend(handles[:3], labels[:3], loc='upper center',
           mode="expand", ncol=3, fontsize=20)
plt.tight_layout()
plt.subplots_adjust(top=0.83)
name = "plots/vs_abserror" + str(z)+".svg"
if saveplots_flag:
    plt.savefig(name, format="svg", dpi=4000)
if showplots_flag:
    plt.show()
plt.cla()
plt.clf()
plt.close()
