''' Main plotting script, plots of most experiments, use bools to control which plots are output'''
import glob
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import matplotlib
import itertools
import os
from math import log10,ceil,floor
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
vs_phi = False
vt_phi = True


def crate_accuracy_results_df(algorithm_names,streamlens,df_max_uniques,
                              df_max_sums,threads,skew_rates,phis,experiment_name,
                              dataset_names,x_axis_name,space_flag,path_prefix="accuracy"):
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
            n="_".join(n)
            for t in thrs:
                for phi in phis:
                    phi_fancy=r"$\frac{{1}}{{{num}}}$".format(num=int(10**-log10(phi))) # display in fraction form
                    for N in streamlens:
                        for u in df_max_uniques:
                            for m in df_max_sums:
                                    for z in srs:
                                        globname="logs/" + path_prefix + "var_"+x_axis_name+"*" + n + "*_accuracy_"+str(t)+"_"+ str(z)+"_"+str(format_float(phi))+"_"+str(m)+"_"+str(u)+"_"+str(N)+"_"+experiment_name+ds+"_accuracy.log"
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
                                            fname_memory = glob.glob("logs/" + path_prefix + "var_skew_*" + n + "*_accuracy_*_" + str(z)+"_"+str(format_float(phi))+"_"+str(m)+"_"+str(u)+"_"+str(N)+"_memory.log")[0]
                                            space, counters = parse_memory(fname_memory)
                                        accudf.loc[len(accudf.index)] = [z,fancy_names[names.index(algname)],scinotation,u,m,pavg,ravg,areavg,t,r"${scale}\times 10^{{-{exp}}}$".format(scale=int(phi*(10**(-floor(log10(phi))))),exp=-floor(log10(phi))),fancy_dataset_names[datasets.index(ds)],float(space),float(counters)]
    return accudf

# Vary skew and vary N (stream length)
if vs_N:
    matplotlib.rcParams['figure.figsize'] = (6, 6.5)  # Dense info resolution
    # matplotlib.rcParams['figure.figsize'] = (6, 5) # Sparse info resolution
    plt.rc('legend', fontsize=26)
    matplotlib.rcParams.update({'font.size': 26})
    ''' Variables: '''
    phis=[0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens = [1000000, 10000000, 100000000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    algonames=names
    palette = [ (0.9333333333333333, 0.5215686274509804, 0.2901960784313726), (0.41568627450980394, 0.8, 0.39215686274509803)]
    ''' ########## '''
    accudf=crate_accuracy_results_df(algonames,streamlens,df_max_uniques,df_max_sums,[24],skew_rates,phis,"varN",[""],"skew",False,path_prefix="accuracy/vsN/")
    accudf.rename(columns={"Streamlength":"Length"},inplace=True)

    # single has no ARE, so we only plot Delegation Space-Saving ARE:
    fig, ax1 = plt.subplots()
    print(accudf)
    lineplot=sns.lineplot(x="Zipf Parameter", y="Average Relative Error", data=accudf[
                                         ((accudf["Algorithm"] == "QPOPSS") |
                                         (accudf["Algorithm"] == "Topkapi")) &
                                         ~((accudf["Length"] == r"$1\times 10^7$") & (accudf["Zipf Parameter"] == 0.5)) & #Remove 0.5 since there are no results for it
                                         ~((accudf["Length"] == r"$1\times 10^8$") & (accudf["Zipf Parameter"] == 0.5)) & 
                                         ~((accudf["Length"] == r"$1\times 10^6$") & (accudf["Zipf Parameter"] == 0.5))
                                        ],
                 markersize=24, linewidth=7, markers=True, style="Length", hue="Algorithm", palette=palette, ax=ax1)

    def rel_err_bound(a, N): return (df_max_sums[0]*24)/N
    ax1.set_yscale("log")

    leg=lineplot.legend(fontsize=24,
        #bbox_to_anchor=(0.73, 0.53 ,0.3,0.5),
        loc='best',
        ncol=2,
        prop={'weight':'normal'},
        markerscale=0.4,
        labelspacing=0.05,
        borderpad=0.1,
        handletextpad=0.1,
        framealpha=0.4,
        handlelength=0.5,
        handleheight=0.5,
        borderaxespad=0,
        columnspacing=0.2)
    [L.set_linewidth(8.0) for L in leg.legendHandles]
    l = Line2D([0],[0],color="w")
    leg.legendHandles.insert(3,l)
    ax1.legend(handles=leg.legendHandles,
        fontsize=24,
        loc='best',
        ncol=2,
        prop={'weight':'normal'},
        markerscale=2.5,
        labelspacing=0.05,
        borderpad=0.1,
        handletextpad=0.1,
        framealpha=0.4,
        handlelength=0.5,
        handleheight=0.5,
        borderaxespad=0,
        columnspacing=0.2
    )
    ax1.set_xlabel("Zipf Parameter")
    ax1.set_ylabel("Average Relative Error")
    ax1.set_ylim(0.00000001,1)
    plt.tight_layout()
    name = "plots/accuracy/vsN/vs_avg_rel_error_varyN.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()


# Vary skew and var df_s and df_u
if vs_dfu_dfs:
    ''' Variables: '''
    phis=[0.00001]
    df_max_uniques = [16, 32, 64, 128]
    df_max_sums = [100, 1000, 10000, 100000] 
    streamlens = [100000000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    algnames=['spacesaving deleg']
    ''' ########## '''
    accudf=crate_accuracy_results_df(algnames,streamlens,df_max_uniques,df_max_sums,[24],skew_rates,phis,"dfsdfu",[""],"skew",False,path_prefix="accuracy/vsdfsdfu/")

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
    name = "plots/accuracy/vsdfudfs/vs_avgrel_finalVarydfsdfu.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
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

    name = "plots/accuracy/vsdfudfs/vs_precision_finalVarydfsdfu.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
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

    name = "plots/accuracy/vsdfudfs/vs_recall_finalVarydfsdfu.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

# Vary skew and vary phi and query rate
if vs_phi:
    ''' Variables: '''
    phis=[0.001,0.0002,0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens = [100000000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    algonames=names

    accudf=crate_accuracy_results_df(algonames,streamlens,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phi",[""],"skew",False,path_prefix="accuracy/vs/")
    matplotlib.rcParams['figure.figsize'] = (6, 5.5) 
    plt.rc('legend', fontsize=26)
    matplotlib.rcParams.update({'font.size': 26})

    fig, ax1 = plt.subplots()
    lineplot=sns.lineplot(x="Zipf Parameter", y="Precision", data=accudf, markersize=13,
                 linewidth=7, markers=True, palette="muted", ax=ax1, style=r"$\phi$",hue="Algorithm", legend='brief')
    ax1.set_xlabel("Skew")
    ax1.set_ylabel("Precision")
    leg=lineplot.legend(fontsize=24,
        loc='best',
        ncol=2,
        prop={'weight':'normal'},
        markerscale=2.5,
        labelspacing=0.05,
        borderpad=0.1,
        handletextpad=0.1,
        framealpha=0.4,
        handlelength=0.5,
        handleheight=0.5,
        borderaxespad=0,
        columnspacing=0.2)
    [L.set_linewidth(5.0) for L in leg.legendHandles]
    plt.tight_layout()
    name = "plots/accuracy/vs/vs_precision_finalVaryQRPhi.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
        plt.savefig(name, format="svg", dpi=4000, bbox_inches='tight')
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()
    #Recall
    fig, ax1 = plt.subplots()
    lineplot=sns.lineplot(x="Zipf Parameter", y="Recall", data=accudf, markersize=13,
                 linewidth=7, markers=True, palette="muted", ax=ax1, style=r"$\phi$",hue="Algorithm", legend=False)
    ax1.set_xlabel("Skew")
    ax1.set_ylabel("Recall")
    plt.tight_layout()

    plt.tight_layout()
    name = "plots/accuracy/vs/vs_recall_finalVaryQRPhi.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
        plt.savefig(name, format="svg", dpi=4000, bbox_inches='tight')
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

# Vary threads
if vt_phi:
    matplotlib.rcParams['figure.figsize'] = (6, 6.5)  # Dense info resolution
    # matplotlib.rcParams['figure.figsize'] = (6, 5) # Sparse info resolution
    plt.rc('legend', fontsize=26)
    matplotlib.rcParams.update({'font.size': 26})
    ''' Variables: '''
    phis=[0.001,0.0002,0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000] 
    streamlens = [100000000]
    threads=[4,8,12,16,20,24]
    algnames=['spacesaving deleg',"topkapi"]
    ''' ########## '''
    accudf=crate_accuracy_results_df(algnames,streamlens,df_max_uniques,df_max_sums,threads,[1.25],phis,"phi",datasets,"threads",False,path_prefix="accuracy/vt/")
    accudf.replace({'CAIDA Flows DirA': 'CAIDA A'}, regex=True,inplace=True)
    accudf.replace({'CAIDA Flows DirB': 'CAIDA B'}, regex=True,inplace=True)
    accudf.replace({'Zipf': 'Zipf 1.25'}, regex=True,inplace=True)
    fig, ax1 = plt.subplots()
    palette = [ (0.9333333333333333, 0.5215686274509804, 0.2901960784313726), (0.41568627450980394, 0.8, 0.39215686274509803)]
    lineplot = sns.lineplot(x="Threads", hue="Algorithm", y="Average Relative Error", data=accudf[
                                                   #(accudf.Algorithm == "DeSS") & 
                                                    accudf[r"$\phi$"] == r"$1\times 10^{-4}$"
                                                ],
                          markersize=24, linewidth=7, markers=True, style="Dataset", ax=ax1, legend='brief',palette=palette)
    leg=lineplot.legend(fontsize=22,
        #bbox_to_anchor=(0.73, 0.53 ,0.3,0.5),
        loc='best',
        ncol=2,
        prop={'weight':'normal'},
        markerscale=0.40,
        labelspacing=0.05,
        borderpad=0.1,
        handletextpad=0.1,
        framealpha=0.4,
        handlelength=0.5,
        handleheight=0.5,
        borderaxespad=0,
        columnspacing=0.2)
    [L.set_linewidth(8.0) for L in leg.legendHandles]
    #lhs=leg.legendHandles
    l = Line2D([0],[0],color="w")
    leg.legendHandles.insert(3,l)
    #fig.legend(handles=leg.legendHandles, title=r"$\phi$")
    ax1.legend(handles=leg.legendHandles,
        fontsize=22,
        loc='best',
        ncol=2,
        prop={'weight':'normal'},
        markerscale=2.5,
        labelspacing=0.05,
        borderpad=0.1,
        handletextpad=0.1,
        framealpha=0.4,
        handlelength=0.5,
        handleheight=0.5,
        borderaxespad=0,
        columnspacing=0.2
    )
    
    ax1.set_ylim(0.00000001,1)
    ax1.set_xlabel("Threads")
    ax1.set_ylabel("Average Relative Error")
    ax1.set_yscale("log")
    plt.tight_layout()
    name = "plots/accuracy/vt/vt_avgre_final.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()

    fig, (ax1, ax2) = plt.subplots(1, 2)
    avgrel = sns.lineplot(x="Threads", hue="Dataset", y="Average Relative Error", data=accudf[(accudf.Algorithm == "Topkapi")],
                          markersize=8, linewidth=3, markers=True, style=r"$\phi$", ax=ax1, legend=False)
    plt.xlabel("Threads")
    ax1.set_ylabel("Average Relative Error")
    prec = sns.lineplot(x="Threads", hue="Dataset", y="Precision", data=accudf[(accudf.Algorithm == "Topkapi")],
                        markersize=8, linewidth=3, markers=True, style=r"$\phi$", ax=ax2)
    plt.xlabel("Threads")
    plt.ylabel("Precision")
    ax2.get_legend().remove()
    fig.legend(loc='upper center', mode="expand",
               ncol=4, borderaxespad=0, labelspacing=0)
    plt.xticks([4, 8, 12, 16, 20, 24])
    plt.tight_layout()
    plt.subplots_adjust(top=0.83)
    name = "plots/accuracy/vt/vt_precision_and_avgre_final.svg"
    if saveplots_flag:
        path = os.path.split(name)[:-1][0]
        if not os.path.exists(path):
            os.makedirs(path)
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()
    plt.close()
