''' Main plotting script, plots of most experiments, use bools to control which plots are output'''
import glob
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import matplotlib
import itertools
from math import log10
import numpy as np
from scipy.special import zeta
from mpl_toolkits.mplot3d import Axes3D
from matplotlib.lines import Line2D
from plotters import average_and_std,parse_accuracy,parse_memory,format_float,saveplots_flag,showplots_flag,datasets,fancy_dataset_names,names,fancy_names

#Matplotlib aesthetic parameters:
matplotlib.rcParams['figure.figsize'] = (11, 6)  # Dense info resolution
# matplotlib.rcParams['figure.figsize'] = (6, 5) # Sparse info resolution
plt.rc('legend', fontsize=16)
matplotlib.rcParams.update({'font.size': 18})

# What experiment(s) should we plot?
vs_N = True
vs_dfu_dfs = False
vs_phi = True
vt_phi = True


def crate_accuracy_results_df(algorithm_names,streamlens,df_max_uniques,df_max_sums,threads,skew_rates,phis,experiment_name,dataset_names,x_axis_name,space_flag):
    columns=["Zipf Parameter", "Algorithm", "Streamlength", "df_u","df_s",
            "Precision","Recall","Average Relative Error", "Threads", r"$\phi$",
            "Dataset","Space","Counters"]
    accudf=pd.DataFrame(columns=columns)
    for ds in dataset_names:
        if ds == "":
            srs = skew_rates
        else:
            srs = [0.5]
        for algname in algorithm_names:
            if algname.find("single") != -1:
                thrs = [1]
            else:
                thrs = threads
            n = algname.split()
            for t in thrs:
                for phi in phis:
                    phi_fancy=r"$\frac{{1}}{{{num}}}$".format(num=int(10**-log10(phi))) # display in fraction form
                    for N in streamlens:
                        for u in df_max_uniques:
                            for m in df_max_sums:
                                    for z in srs:
                                        globname="logs/var_"+x_axis_name+"*" + n[0] + "_" + n[1] + "*_accuracy_"+str(t)+"_"+ str(z)+"_"+str(format_float(phi))+"_"+str(m)+"_"+str(u)+"_"+str(N)+"_"+experiment_name+ds+"_accuracy.log"
                                        
                                        print(globname)
                                        file=glob.glob(globname)[0]
                                        print(file)
                                        prec, rec, are = parse_accuracy(file)
                                        pavg, pstd = average_and_std(prec)
                                        ravg, rstd = average_and_std(rec)
                                        areavg, arestd = average_and_std(are)
                                        ''' Scientific notation of streamlength '''
                                        exponent = int(log10(N))
                                        scalar = int(N/(10**exponent))
                                        scinotation = r"$" +  str(scalar) + r"\times 10^" + str(exponent) + r"$" # display streamlen in scinotation
                                        counters=np.nan
                                        space=np.nan                  
                                        if space_flag:
                                            fname_memory = glob.glob("logs/var_skew_*" + n[0] + "_" + n[1] + "*_accuracy_*_" + str(z)+"_"+str(format_float(phi))+"_"+str(m)+"_"+str(u)+"_"+str(N)+"_memory.log")[0]
                                            space, counters = parse_memory(fname_memory)
                                        accudf.loc[len(accudf.index)] = [z,fancy_names[names.index(algname)],scinotation,u,m,pavg,ravg,areavg,t,phi_fancy,fancy_dataset_names[datasets.index(ds)],float(space),float(counters)]
    return accudf

# Vary skew and vary N (stream length)
if vs_N:
    ''' Variables: '''
    phis=[0.00001]
    df_max_uniques = [64]
    df_max_sums = [1000]
    streamlens = [1000000, 10000000, 30000000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    algonames=names
    ''' ########## '''
    accudf=crate_accuracy_results_df(algonames,streamlens,df_max_uniques,df_max_sums,[24],skew_rates,phis,"varN",[""],"skew",False)

    # single has no ARE, so we only plot Delegation Space-Saving ARE:
    fig, ax = plt.subplots()
    sns.lineplot(x="Zipf Parameter", y="Average Relative Error", data=accudf[accudf.Algorithm == "Delegation Space-Saving"],
                 markersize=8, linewidth=3, markers=True, style="Algorithm", hue="Streamlength", palette="muted", ax=ax)

    def rel_err_bound(a, N): return (df_max_sums[0]*24)/N
    xs = np.arange(0.5, 3.25, 0.25)
    sns.lineplot(xs, list(map(rel_err_bound, xs, [1000000]*11)), 
                linestyle="dashed", color="b", palette="muted", ax=ax)

    sns.lineplot(xs, list(map(rel_err_bound, xs, [10000000]*11)),
                linestyle="dashed", color="orange", palette="muted", ax=ax)
    sns.lineplot(xs, list(map(rel_err_bound, xs, [30000000]*11)),
                 linestyle="dashed", color="g", palette="muted", ax=ax)

    handles, labels = ax.get_legend_handles_labels()
    entry = Line2D([0], [0], linestyle="dashed",
                   lw=1.8, ms=9, mew=0.2, color='0')
    handles.append(entry)
    labels.append(r"$\frac{mT}{N}$")

    plt.legend(handles, labels)

    plt.xlabel("Skew")
    plt.ylabel("Average Relative Error")
    plt.tight_layout()
    name = "plots/vs_avg_rel_error_final_varyN.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

    fig, (ax1, ax2) = plt.subplots(1, 2)

    prec = sns.lineplot(x="Zipf Parameter", y="Precision", data=accudf, markersize=8, linewidth=3,
                        markers=True, style="Algorithm", hue="Streamlength", palette="muted", ax=ax1)
    ax1.set_xlabel("Skew")
    ax1.set_ylabel("Precision")
    ax1.set_ylim([0.6, 1.01])

    rec = sns.lineplot(x="Zipf Parameter", y="Recall", data=accudf[accudf.Algorithm == "Delegation Space-Saving"],
                       markersize=8, linewidth=3, markers=True, style="Algorithm", hue="Streamlength", palette="muted", ax=ax2, legend=False)
    ax2.set_xlabel("Skew")
    ax2.set_ylabel("Recall")
    ax2.set_ylim([0.6, 1.01])
    ax1.get_legend().remove()
    ax4 = plt.axes([0.75, 0.4, .15, .15])
    sns.lineplot(x="Zipf Parameter", y="Recall", data=accudf[accudf.Algorithm == "Delegation Space-Saving"],
                 markersize=8, linewidth=3, markers=True, style="Algorithm", hue="Streamlength", palette="muted", ax=ax4, legend=False)
    ax4.set_title('zoom')
    ax4.set_xlabel('')
    ax4.set_ylabel('')
    ax4.set_xlim([0.75, 1.5])
    ax4.set_ylim([0.9975, 1.001])
    ax2.indicate_inset_zoom(ax4, edgecolor="black")

    # Insert fake entry in legend (to facilitate spacing)
    handles, labels = ax1.get_legend_handles_labels()
    entry = matplotlib.patches.Rectangle((0, 0), 1, 1, fill=False, edgecolor='none',
                                         visible=False)
    handles.insert(5, entry)
    labels.insert(5, "")

    fig.legend(handles, labels, loc='upper center', mode="expand", ncol=4)
    fig.tight_layout()
    plt.subplots_adjust(top=0.83)

    name = "plots/vs_recall_final_varyN.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf
    plt.close()

# Vary skew and var df_s and df_u
if vs_dfu_dfs:
    ''' Variables: '''
    phis=[0.00001]
    df_max_uniques = [16, 32, 64, 128]
    df_max_sums = [100, 1000, 10000, 100000] 
    streamlens = [30000000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    algnames=['spacesaving deleg']
    ''' ########## '''
    accudf=crate_accuracy_results_df(algnames,streamlens,df_max_uniques,df_max_sums,[24],skew_rates,phis,"dfsdfu",[""],"skew",False)

    ''' Matplotlib aesthetic parameters 3dplot '''
    plt.rcParams['xtick.major.pad'] = 12
    plt.rcParams['ytick.major.pad'] = 12
    plt.rcParams['axes.labelpad'] = 35

    fig = plt.gcf()
    ax = Axes3D(fig)
    
    #All combinations of df_s and df_u
    series = accudf[['df_s', 'df_u']].apply(tuple, axis=1)
    strs = series.apply(lambda x: str(int(x[0]))+","+str(int(x[1])))
    combs = [str(df_s) + "," + str(df_u) for df_s,
             df_u in list(itertools.product(df_max_sums, df_max_uniques))]
    strs = strs.apply(lambda x: combs.index(x))

    #Plot Avg relative error
    ax.plot_trisurf(strs, accudf["Zipf Parameter"],
                    accudf["Average Relative Error"], cmap=plt.cm.viridis, linewidth=0.2)
    plt.xticks(range(len(combs)), combs)
    ax.set_xticklabels(combs, rotation=-15, verticalalignment='baseline',
                       horizontalalignment='left')
    ax.set_xlabel(r"$df_s,df_u$")
    ax.set_ylabel("Skew")
    ax.set_zlabel("Average Relative Error")
    plt.tight_layout()
    name = "plots/vs_avgrel_finalVarydfsdfu.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

    fig = plt.gcf()
    ax = Axes3D(fig)

    #Plot Precision
    ax.plot_trisurf(strs, accudf["Zipf Parameter"],
                    accudf["Precision"], cmap=plt.cm.viridis, linewidth=0.2)
    plt.xticks(range(len(combs)), combs)
    ax.set_xticklabels(combs, rotation=-15, verticalalignment='baseline',
                       horizontalalignment='left')
    ax.set_xlabel(r"$df_s,df_u$")
    ax.set_ylabel("Skew")
    ax.set_zlabel("Precision")

    name = "plots/vs_precision_finalVarydfsdfu.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

    fig = plt.gcf()
    ax = Axes3D(fig)

    #Plot Recall
    ax.plot_trisurf(accudf["Zipf Parameter"], strs,
                    accudf["Recall"], cmap=plt.cm.viridis, linewidth=0.2)
    plt.yticks(range(len(combs)), combs)
    ax.set_yticklabels(combs, rotation=-15, verticalalignment='baseline',
                       horizontalalignment='left')
    ax.set_ylabel(r"$df_s,df_u$")
    ax.set_xlabel("Skew")
    ax.set_zlabel("Recall")

    name = "plots/vs_recall_finalVarydfsdfu.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

# Vary skew and vary phi and query rate
if vs_phi:
    ''' Variables: '''
    phis=[0.001,0.0001,0.00001]
    df_max_uniques = [64]
    df_max_sums = [1000]
    streamlens = [30000000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    algonames=names
    ''' ########## '''
    accudf=crate_accuracy_results_df(algonames,streamlens,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phi",[""],"skew",True)

    # Plot space difference
    newdf = pd.DataFrame(columns=["delegspace", "singlespace"])
    newdf["singlespace"]=accudf[accudf["Algorithm"]=="Single Space-Saving"]["Space"]
    newdf["delegspace"]=accudf[accudf["Algorithm"]=="Delegation Space-Saving"]["Space"].reset_index().drop("index",axis=1) 
    newdf[r"$\phi$"]=accudf[accudf["Algorithm"]=="Single Space-Saving"][r"$\phi$"]
    newdf["Zipf Parameter"]=accudf[accudf["Algorithm"]=="Single Space-Saving"]["Zipf Parameter"]
    newdf['Percent additional bytes']=newdf['delegspace'] / newdf['singlespace']   #newdf['delegspace']-newdf['singlespace']
    newdf["delegspace"]=accudf[accudf["Algorithm"]=="Single Space-Saving"]["Space"]/ 100000

    mutedblue = sns.color_palette(['#4878d0', '#4878d0', '#4878d0'])
    mutedorange = sns.color_palette(['#ee854a', '#ee854a', '#ee854a'])
    mutedgreen = sns.color_palette(['#6acc64'])
    fig, ax = plt.subplots()
    ax.set_ylabel("Relative memory increase", color="#4878d0")
    space = sns.lineplot(x="Zipf Parameter", hue=r"$\phi$", y="Percent additional bytes", data=newdf,
                         markersize=8, linewidth=3, markers=True, palette=mutedblue, style=r"$\phi$", ax=ax, legend=False)
    ax2 = plt.twinx()
    ax2.set_ylabel("Delegation Space-Saving memory (MB)", color="#ee854a")
    sns.lineplot(x="Zipf Parameter", hue=r"$\phi$", y="delegspace", data=newdf, markersize=8,
                 linewidth=3, markers=True, palette=mutedorange, style=r"$\phi$", ax=ax2, legend=False)
    sns.lineplot(x="Zipf Parameter", hue=r"$\phi$", y="delegspace", data=newdf, markersize=8,
                 linewidth=3, markers=True, palette=mutedorange, style=r"$\phi$", legend=False)
    ax4 = plt.axes([0.14, 0.4, .16, .16])
    sns.lineplot(x="Zipf Parameter", hue=r"$\phi$", y="Percent additional bytes", data=newdf,
                 markersize=8, linewidth=3, markers=True, palette=mutedblue, style=r"$\phi$", ax=ax4, legend=False)

    ax4.set_title('zoom')
    ax4.set_xlim([0.45, 1.05])
    ax4.set_ylim([0.9, 2.7])

    ax4.set_xlabel('')
    ax4.set_ylabel('')
    ax.indicate_inset_zoom(ax4, edgecolor="black")
    
    legend_elements = [
        Line2D([0], [0], marker='o',  lw=1.7,
               ms=9, mew=0.2, color='0', label=r'$\frac{1}{1000}$'),
        Line2D([0], [0], marker='X', linestyle='dashed', lw=1.7,
               ms=9, mew=0.2, color='0', label=r'$\frac{1}{10000}$'),
        Line2D([0], [0], marker='s', linestyle='dotted', lw=1.5, ms=9, mew=0.2, color='0', label=r'$\frac{1}{100000}$')]
    fig.legend(handles=legend_elements, loc="upper center", title=r"$\phi$")
    
    plt.tight_layout()
    name = "plots/vs_space_finalVaryQRPhi.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

    fig, (ax1, ax2) = plt.subplots(1, 2)
    sns.lineplot(x="Zipf Parameter", hue=r"$\phi$", y="Average Relative Error", data=accudf, markersize=8,
                 linewidth=3, markers=True, palette="muted", ax=ax1, style="Algorithm", legend=False)
    ax1.set_xlabel("Skew")
    ax1.set_ylabel("Average Relative Error")

    prec = sns.lineplot(x="Zipf Parameter", y="Precision", data=accudf, markersize=8,
                        linewidth=3, markers=True, style="Algorithm", hue=r"$\phi$", palette="muted", ax=ax2)
    ax2.set_xlabel("Skew")
    ax2.set_ylabel("Precision")
    ax2.get_legend().remove()
    fig.legend(loc='upper center', mode="expand",
               ncol=4, borderaxespad=0, labelspacing=0)
    plt.tight_layout()
    plt.subplots_adjust(top=0.83)
    name = "plots/vs_precision_finalVaryQRPhi.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000, bbox_inches='tight')
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

# Vary threads
if vt_phi:
    ''' Variables: '''
    phis=[0.001,0.0001,0.00001]
    df_max_uniques = [64]
    df_max_sums = [1000] 
    streamlens = [30000000]
    threads=[4,8,12,16,20,24]
    algnames=['spacesaving deleg']
    ''' ########## '''
    accudf=crate_accuracy_results_df(algnames,streamlens,df_max_uniques,df_max_sums,threads,[1.25],phis,"phi",datasets,"threads",False)

    fig, (ax1, ax2) = plt.subplots(1, 2)
    avgrel = sns.lineplot(x="Threads", hue="Dataset", y="Average Relative Error", data=accudf,
                          markersize=8, linewidth=3, markers=True, style=r"$\phi$", ax=ax1, legend=False)
    plt.xlabel("Threads")
    ax1.set_ylabel("Average Relative Error")
    prec = sns.lineplot(x="Threads", hue="Dataset", y="Precision", data=accudf,
                        markersize=8, linewidth=3, markers=True, style=r"$\phi$", ax=ax2)
    plt.xlabel("Threads")
    plt.ylabel("Precision")
    ax2.get_legend().remove()
    fig.legend(loc='upper center', mode="expand",
               ncol=4, borderaxespad=0, labelspacing=0)
    plt.xticks([4, 8, 12, 16, 20, 24])
    plt.tight_layout()
    plt.subplots_adjust(top=0.83)
    name = "plots/vt_precision_and_avgre_final.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()
