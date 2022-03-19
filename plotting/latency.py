from matplotlib import pyplot as plt
import seaborn as sns
import matplotlib.ticker as plticker
import numpy as np
import pandas as pd
import matplotlib
import glob
from matplotlib.lines import Line2D
from math import log10
from plotters import average_and_std,parse_latency,format_float,RUNTIME,names,fancy_names,datasets,fancy_dataset_names,showplots_flag,saveplots_flag

#Matplotlib aesthetic parameters:
matplotlib.rcParams['figure.figsize'] = (11, 6)  # Dense info resolution
# matplotlib.rcParams['figure.figsize'] = (6, 5) # Sparse info resolution
plt.rc('legend', fontsize=24)
matplotlib.rcParams.update({'font.size': 24})

# What parameter should be varied?
vs_phi_qr = True
vt_phi_qr = True

# What is the metric?
throughput = True
speedupthroughput = True
fancy_names = ["Space-Saving w/ Max-Heap","Delegation Space-Saving","Delegation Space-Saving w/ Max-Heap","Space-Saving"]
def crate_performance_results_df(algorithm_names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,skew_rates,phis,experiment_name,dataset_names,x_axis_name,maxheap_flag):
    columns = ["Zipf Parameter", "Algorithm","Latency",
                "df_s","df_u","phi", r"$\phi$",
                "Query Rate","Dataset","Threads"]
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
                                        if n == ["topkapi"]:
                                            globname="logs/"+x_axis_name+"_cm_topkapi_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_latency.log"
                                        else:
                                            globname="logs/"+x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_latency.log"
                                        print(globname)                                        
                                        file=glob.glob(globname)[0]
                                        data = parse_latency(file)
                                        latavg, latstd = average_and_std(data)                          
                                        queryRate=float(0 if qr==0 else float(qr)/(10000))
                                        perfdf.loc[len(perfdf.index)] = [z, fancy_names[names.index(algname)], float(latavg),
                                                                        df_max_sum,df_max_unique,float(phi),r"$\frac{{1}}{{{num}}}$".format(num=int(10**-log10(phi))),
                                                                        queryRate,fancy_dataset_names[datasets.index(ds)],int(t)]
    return perfdf


# Vary skew, phi and query rate
if vs_phi_qr:
    ''' Variables: '''
    phis = [0.001, 0.0001, 0.00001]
    df_max_uniques = [64]
    df_max_sums = [1000]
    streamlens=[30000000]
    query_rates = [10, 100, 1000]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    names=["spacesaving single_maxheap", "spacesaving deleg","spacesaving deleg_maxheap","spacesaving single"]
    datasets=[""]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phiqr",datasets,"skew",True)
    if throughput:

        # Latency with 0.1% queries single:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["Dataset"] == "Zipf") & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "Space-Saving") | (perfdf["Algorithm"] == "Space-Saving w/ Max-Heap"))], markersize=10,
                     linewidth=7, linestyle="--", markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Skew")
        plt.ylabel(r"Latency ($\mu$sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        fig.legend(loc='upper center', ncol=1, borderaxespad=0, labelspacing=0)
        plt.subplots_adjust(top=0.83)
        loc = plticker.MultipleLocator(base=20) # this locator puts ticks at regular intervals
        ax.yaxis.set_major_locator(loc)
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/latency/zipf_single.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()


        # Latency with 0.1% queries deleg:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["Dataset"] == "Zipf") & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "Delegation Space-Saving") | (perfdf["Algorithm"] == "Delegation Space-Saving w/ Max-Heap"))], markersize=10,
                     linewidth=7,linestyle="--", markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Skew")
        plt.ylabel(r"Latency ($\mu$sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        fig.legend(loc='upper center', ncol=1, borderaxespad=0, labelspacing=0)
        plt.subplots_adjust(top=0.83)
        loc = plticker.MultipleLocator(base=80) # this locator puts ticks at regular intervals
        ax.yaxis.set_major_locator(loc)
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/latency/zipf_deleg.svg"
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
    query_rates = [10, 100, 1000]
    threads=[4,8,12,16,20,24]
    names=["spacesaving single_maxheap", "spacesaving deleg","spacesaving deleg_maxheap"]
    datasets=["flows_dirB"]
    fancy_dataset_names = ["CAIDA Flows DirB"]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,[1],phis,"phiqr",datasets,"threads",True)

    print(perfdf)
    if throughput:
        # Latency with 0.1% queries deleg diff threads:
        fig, ax = plt.subplots()
        sns.lineplot(x="Threads", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["Dataset"] == "CAIDA Flows DirB") & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "Delegation Space-Saving") | (perfdf["Algorithm"] == "Delegation Space-Saving w/ Max-Heap"))], markersize=10,
                     linewidth=7,linestyle="--", markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Threads")
        plt.ylabel(r"Latency ($\mu$sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        fig.legend(loc='upper center', ncol=1, borderaxespad=0, labelspacing=0)
        plt.subplots_adjust(top=0.83)
        loc = plticker.MultipleLocator(base=80) # this locator puts ticks at regular intervals
        ax.yaxis.set_major_locator(loc)
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/latency/threads.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()
