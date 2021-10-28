from matplotlib import pyplot as plt
import seaborn as sns
import numpy as np
import pandas as pd
import matplotlib
import glob
from matplotlib.lines import Line2D
from math import log10
from plotters import average_and_std,parse_throughput,format_float,RUNTIME,names,fancy_names,datasets,fancy_dataset_names,showplots_flag,saveplots_flag

#Matplotlib aesthetic parameters:
matplotlib.rcParams['figure.figsize'] = (11, 6)  # Dense info resolution
# matplotlib.rcParams['figure.figsize'] = (6, 5) # Sparse info resolution
plt.rc('legend', fontsize=16)
matplotlib.rcParams.update({'font.size': 18})

# What parameter should be varied?
vs_dfu_dfs = True
vs_phi_qr = True
vt_phi_qr = True

# What is the metric?
throughput = True
speedupthroughput = True

def crate_performance_results_df(algorithm_names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,skew_rates,phis,experiment_name,dataset_names,x_axis_name,speedup_flag):
    columns = ["Zipf Parameter", "Algorithm","Throughput","Streamlength",
                "Queries","df_s","df_u","phi", r"$\phi$",
                "Query Rate","Speedup","Dataset","Threads"]
    perfdf=pd.DataFrame(columns=columns)
    for ds in dataset_names:
        if ds == "": # if non-real data, aka synthetic (zipf)
            srs = skew_rates
        else:  
            srs=[0.5]
        for algname in algorithm_names:
            if algname.find("single") != -1:
                thrs = [1]
            else:
                thrs = threads
            n = algname.split()
            for t in thrs:
                for N in streamlens:
                    for qr in query_rates:
                        for phi in phis:
                            for df_max_sum in df_max_sums:
                                for df_max_unique in df_max_uniques:
                                    for z in srs:
                                        globname="logs/"+x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                        file=glob.glob(globname)[0]
                                        data = parse_throughput(file)
                                        tpavg, tpstd = average_and_std(data)      
                                        speedup = np.nan
                                        if speedup_flag:
                                            if n[1] == "deleg":
                                                single_throughput = perfdf[
                                                        (perfdf["Zipf Parameter"] == z) & 
                                                        (perfdf["Query Rate"] == (0 if qr==0 else 1/float(qr))) & 
                                                        (perfdf["Algorithm"] == "Single Space-Saving") & 
                                                        (perfdf["Dataset"] == fancy_dataset_names[datasets.index(ds)]) &
                                                        (perfdf["phi"] == float(phi))]
                                                single_throughput=single_throughput.Throughput.values[0]
                                                speedup = float(float(tpavg) / float(single_throughput))
                                        queryRate=float(0 if qr==0 else 1/float(qr))
                                        perfdf.loc[len(perfdf.index)] = [z, fancy_names[names.index(algname)], float(tpavg), float(tpavg)*1000000*RUNTIME, 
                                                                        float(tpavg)*RUNTIME*1000000*float(qr)*1/10000,df_max_sum,df_max_unique,float(phi),r"$\frac{{1}}{{{num}}}$".format(num=int(10**-log10(phi))),
                                                                        queryRate,speedup,fancy_dataset_names[datasets.index(ds)],int(t)]
    return perfdf


# Show influence of df_s and df_u over skew
if vs_dfu_dfs:
    ''' Variables: '''
    phis = [0.00001]
    df_max_uniques = [16, 32, 64, 128]
    df_max_sums = [100, 1000, 10000, 100000]
    streamlens=[30000000]
    query_rates = [100]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    ''' ########## '''

    perfdf=crate_performance_results_df(["spacesaving deleg"],streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"dfsdfu",[""],"skew",False)
    if throughput:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf, markersize=8, linewidth=3, markers=True,
                     style=perfdf["df_u"], hue=perfdf["df_s"], palette="muted", legend="full", ax=ax)
        ax2 = plt.axes([0.19, 0.40, .2, .2])
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf, markersize=8, linewidth=3, markers=True,
                     style=perfdf["df_u"], hue=perfdf["df_s"], palette="muted", legend=False, ax=ax2)
        ax2.set_title('zoom')
        ax2.set_xlim([0.45, 1.05])
        ax2.set_ylim([5, 13])

        ax.axes.get_legend().remove()
        ax2.set_xlabel('')
        ax2.set_ylabel('')
        ax2.set_xticks([0.5, 0.75, 1])
        ax.indicate_inset_zoom(ax2, edgecolor="black")
        ax.set_xlabel("Skew")
        ax.set_ylabel("Throughput (Million Inserts/sec)")
        fig.legend(ncol=2, bbox_to_anchor=(0.9, 0.43), loc="center right")
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_performance_throughput_final_dfu_dfs.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()


# Vary skew, phi and query rate
if vs_phi_qr:
    ''' Variables: '''
    phis = [0.001, 0.0001, 0.00001]
    df_max_uniques = [64]
    df_max_sums = [1000]
    streamlens=[30000000]
    query_rates = [0, 10, 100, 1000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phiqr",datasets,"skew",True)
    if throughput:

        #Throughput with 0 queries:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf[(perfdf["Query Rate"] == 0) & (perfdf["Dataset"] == "Zipf")], markersize=8,
                     linewidth=3, markers=True, style="Algorithm", hue=r"$\phi$", palette="muted", ax=ax)
        plt.xlabel("Skew")
        plt.ylabel("Throughput (Million Inserts/sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        fig.legend(loc='upper center',
                   mode="expand", ncol=4, borderaxespad=0, labelspacing=0)
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_performance_throughput_finalphiqr_no_quer.svg"
        ax.set_ylim([-1, 700])
        plt.subplots_adjust(top=0.83)
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

        # Throughput with 0.1% queries:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["Dataset"] == "Zipf")], markersize=8,
                     linewidth=3, markers=True, style="Algorithm", hue=r"$\phi$", palette="muted", ax=ax)
        plt.xlabel("Skew")
        plt.ylabel("Throughput (Million Inserts/sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        ax.set_ylim([-1, 700])
        fig.legend(loc='upper center',
                   mode="expand", ncol=4, borderaxespad=0, labelspacing=0)
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_performance_throughput_finalphiqr_no_quer.svg"
        plt.subplots_adjust(top=0.83)
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_performance_throughput_finalphiqr_0.1_quer.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

    # Performance over different skew rates for 24 threads speedup
    if speedupthroughput:
        fig, axs = plt.subplots(3, 1, sharex=True, sharey=True)
        mutedblue = sns.color_palette(['#4878d0'])
        mutedorange = sns.color_palette(['#ee854a'])
        mutedgreen = sns.color_palette(['#6acc64'])
        ls1 = sns.lineplot(x="Zipf Parameter", y="Speedup", data=perfdf[(perfdf[r"$\phi$"] == r'$\frac{1}{1000}$') &
                                                         (perfdf["Algorithm"] == "Delegation Space-Saving") &
                                                         (perfdf["Dataset"] == "Zipf")], markersize=8, linewidth=3,
                           markers=True, hue=r"$\phi$", style="Query Rate", legend=False, palette=mutedblue, ax=axs[0])
        ls2 = sns.lineplot(x="Zipf Parameter", y="Speedup", data=perfdf[(perfdf[r"$\phi$"] == r'$\frac{1}{10000}$') &
                                                         (perfdf["Algorithm"] == "Delegation Space-Saving") &
                                                         (perfdf["Dataset"] == "Zipf")], markersize=8, linewidth=3,
                           markers=True, hue=r"$\phi$", style="Query Rate", legend=False, palette=mutedorange, ax=axs[1])
        ls3 = sns.lineplot(x="Zipf Parameter", y="Speedup", data=perfdf[(perfdf[r"$\phi$"] == r'$\frac{1}{100000}$') &
                                                         (perfdf["Algorithm"] == "Delegation Space-Saving") &
                                                         (perfdf["Dataset"] == "Zipf")], markersize=8, linewidth=3,
                           markers=True, hue=r"$\phi$", style="Query Rate", legend=False, palette=mutedgreen, ax=axs[2])
        plt.xlabel("Skew")
        plt.ylabel("Speedup")
        plt.yticks([4, 8, 12, 16, 20])
        plt.ylim([0, 20])
        
        legend_elements = [Line2D([0], [0], color='#4878d0', lw=3, label=r'$\phi=\frac{1}{1000}$'),
                           Line2D([0], [0], color='#ee854a', lw=3,
                                  label=r'$\phi=\frac{1}{10000}$'),
                           Line2D([0], [0], color='#6acc64', lw=3,
                                  label=r'$\phi=\frac{1}{100000}$'),
                           matplotlib.patches.Rectangle((0, 0), 1, 1, fill=False, edgecolor='none',
                                                        visible=False),
                           Line2D([0], [0], marker='o', lw=1.8, ms=9,
                                  mew=0.2, color='0', label='% queries=0'),
                           Line2D([0], [0], marker='X', linestyle='dashed', lw=1.7,
                                  ms=9, mew=0.2, color='0', label='% queries=0.001'),
                           Line2D([0], [0], marker='s', linestyle='dotted', lw=1.7,
                                  ms=9, mew=0.2, color='0', label='% queries=0.01'),
                           Line2D([0], [0], marker='P', linestyle='dashdot', lw=1.5, ms=9, mew=0.2, color='0', label='% queries=0.1')]
        fig.legend(handles=legend_elements, loc='upper center',
                   mode="expand", ncol=4, borderaxespad=0, labelspacing=0)
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vs_performance_speedup_finalphiqr.svg"
        axs[0].grid(axis="y")
        axs[1].grid(axis="y")
        axs[2].grid(axis="y")
        plt.tight_layout()
        plt.subplots_adjust(top=0.83)
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

        ###Real Data Speedup
        catplot = sns.catplot(y="Speedup", x="Query Rate", data=perfdf[((perfdf["Dataset"] == "CAIDA Dest. IPv4") |
                                                                       (perfdf["Dataset"] == "CAIDA Dest. Port")) &
                                                                       (perfdf["Algorithm"] == "Delegation Space-Saving")], 
                                                            hue=r"$\phi$", col="Dataset", marker="$\_$",
                              s=30, jitter=False, height=8, aspect=3.8/8, legend_out=True, legend=True, sharey=True)
        catplot.despine(right=False, top=False)
        plt.xlabel("Query Rate")
        catplot.fig.get_axes()[0].set_yticks(list(range(4, 15)))
        catplot.fig.get_axes()[1].set_yticks(list(range(4, 15)))
        plt.ylabel("Speedup")
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vp_performance_speedup_finalreal.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()
    

# Vary threads, phi and query rate
if vt_phi_qr:
    ''' Variables: '''
    phis = [0.001, 0.0001, 0.00001]
    df_max_uniques = [64]
    df_max_sums = [1000]
    streamlens=[30000000]
    query_rates = [0, 10, 100, 1000]
    threads=[4,8,12,16,20,24]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,[1.25],phis,"phiqr",datasets,"threads",True)
    if throughput:
        sns.lineplot(x="Threads", y="Throughput",  data=perfdf[(perfdf["Dataset"] == "CAIDA Dest. Port") &
                                                                       (perfdf["Algorithm"] == "Delegation Space-Saving")], markersize=8, linewidth=3, markers=True,
                     style="Query Rate", hue=r"$\phi$", palette="muted", legend="full")
        plt.xlabel("Threads")
        plt.ylabel("Throughput (Million Inserts/sec)")
        plt.tight_layout()
        name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vt_performance_throughput_finalphiqr.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

    # Performance over different skew rates for 24 threads speedup
    if speedupthroughput:
        for ds in datasets:
            fig, axs = plt.subplots(3, 1, sharex=True, sharey=True)
            mutedblue = sns.color_palette(['#4878d0'])
            mutedorange = sns.color_palette(['#ee854a'])
            mutedgreen = sns.color_palette(['#6acc64'])
            ls1 = sns.lineplot(x="Threads", y="Speedup", data=perfdf[(perfdf[r"$\phi$"] == r'$\frac{1}{1000}$') &
                                                            (perfdf["Dataset"] == fancy_dataset_names[datasets.index(ds)]) &
                                                            (perfdf["Algorithm"] == "Delegation Space-Saving")], markersize=8, linewidth=3,
                            markers=True, hue=r"$\phi$", style="Query Rate", legend=False, palette=mutedblue, ax=axs[0])
            ls2 = sns.lineplot(x="Threads", y="Speedup", data=perfdf[(perfdf[r"$\phi$"] == r'$\frac{1}{1000}$') &
                                                            (perfdf["Dataset"] == fancy_dataset_names[datasets.index(ds)]) &
                                                            (perfdf["Algorithm"] == "Delegation Space-Saving")], markersize=8, linewidth=3,
                            markers=True, hue=r"$\phi$", style="Query Rate", legend=False, palette=mutedorange, ax=axs[1])
            ls3 = sns.lineplot(x="Threads", y="Speedup", data=perfdf[(perfdf[r"$\phi$"] == r'$\frac{1}{1000}$') &
                                                            (perfdf["Dataset"] == fancy_dataset_names[datasets.index(ds)]) &
                                                            (perfdf["Algorithm"] == "Delegation Space-Saving")], markersize=8, linewidth=3,
                            markers=True, hue=r"$\phi$", style="Query Rate", legend=False, palette=mutedgreen, ax=axs[2])
            plt.xlabel("Threads")
            plt.ylabel("Speedup")
            plt.xlim([3, 25])
            plt.xticks([4, 8, 12, 16, 20, 24])
            plt.yticks([4, 8, 12, 16, 20])
            plt.ylim([0, 20])
            legend_elements = [Line2D([0], [0], color='#4878d0', lw=3, label=r'$\phi=\frac{1}{1000}$'),
                            Line2D([0], [0], color='#ee854a', lw=3,
                                    label=r'$\phi=\frac{1}{10000}$'),
                            Line2D([0], [0], color='#6acc64', lw=3,
                                    label=r'$\phi=\frac{1}{100000}$'),
                            matplotlib.patches.Rectangle((0, 0), 1, 1, fill=False, edgecolor='none',
                                                            visible=False),
                            Line2D([0], [0], marker='o', lw=1.8, ms=9,
                                    mew=0.2, color='0', label='% queries=0'),
                            Line2D([0], [0], marker='X', linestyle='dashed', lw=1.7,
                                    ms=9, mew=0.2, color='0', label='% queries=0.001'),
                            Line2D([0], [0], marker='s', linestyle='dotted', lw=1.7,
                                    ms=9, mew=0.2, color='0', label='% queries=0.01'),
                            Line2D([0], [0], marker='P', linestyle='dashdot', lw=1.5, ms=9, mew=0.2, color='0', label='% queries=0.1')]
            fig.legend(handles=legend_elements, loc='upper center',
                    mode="expand", ncol=4, borderaxespad=0, labelspacing=0)
            name = "/home/victor/git/DelegationSketchTopK-singlequery/plots/vt_performance_speedup_finalphiqr"+ds+".svg"
            axs[0].grid(axis="y")
            axs[1].grid(axis="y")
            axs[2].grid(axis="y")
            plt.tight_layout()
            plt.subplots_adjust(top=0.83)
            if saveplots_flag:
                plt.savefig(name, format="svg", dpi=4000)
            if showplots_flag:
                plt.show()
            plt.clf()
            plt.cla()
            plt.close()