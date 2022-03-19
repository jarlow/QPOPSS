from matplotlib import pyplot as plt
import seaborn as sns
import matplotlib.ticker as plticker
import numpy as np
import pandas as pd
import matplotlib
import glob
from matplotlib.lines import Line2D
from math import log10
from plotters import average_and_std,parse_latency,format_float,RUNTIME,names,fancy_names,datasets,fancy_dataset_names, parse_throughput,showplots_flag,saveplots_flag

#Matplotlib aesthetic parameters:
matplotlib.rcParams['figure.figsize'] = (8, 7)  # Dense info resolution
# matplotlib.rcParams['figure.figsize'] = (6, 5) # Sparse info resolution
plt.rc('legend', fontsize=26)
matplotlib.rcParams.update({'font.size': 34})

# What parameter should be varied?
vs_phi_qr = False
vt_phi_qr = True

# What is the metric?
throughput = True
speedupthroughput = True
fancy_names = ["DeSS","DeSS+MH"]
def crate_performance_results_df(algorithm_names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,skew_rates,phis,experiment_name,dataset_names,x_axis_name,maxheap_flag):
    columns = ["Zipf Parameter", "Algorithm","Latency","Throughput",
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
                                            globname="logs/"+x_axis_name+"_cm_topkapi_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                        else:
                                            globname="logs/"+x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"                                     
                                        print(globname)
                                        file=glob.glob(globname)[0]
                                        throughput = parse_throughput(file)
                                        throughputavg, _ = average_and_std(throughput)   
                                        if n == ["topkapi"]:
                                            globname="logs/"+x_axis_name+"_cm_topkapi_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_latency.log"
                                        else:
                                            globname="logs/"+x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_latency.log"
                                        
                                        print(globname)
                                        file=glob.glob(globname)[0]
                                        latency = parse_latency(file)
                                        latencyavg,_ = average_and_std(latency)                          
                                        queryRate=float(0 if qr==0 else float(qr)/(10000))
                                        perfdf.loc[len(perfdf.index)] = [z, fancy_names[names.index(algname)], float(latencyavg),float(throughputavg),
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
    names=["spacesaving deleg","spacesaving deleg_maxheap"]
    datasets=[""]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phiqr",datasets,"skew",True)
    if throughput:
        # Latency with 0.1% queries deleg:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["Dataset"] == "Zipf") & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "DeSS") | (perfdf["Algorithm"] == "DeSS+MH"))], markersize=10,
                     linewidth=7, markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Skew")
        plt.ylabel(r"Throughput (Mops/sec)")
        plt.tight_layout()
        handles, labels = ax.get_legend_handles_labels()
        handles = handles[0:]
        labels=labels[0:]
        ax.axes.get_legend().remove()
        leg=fig.legend(handles, labels, loc="right", ncol=1)
        for legobj in leg.legendHandles:
            legobj.set_linewidth(7)
        #plt.subplots_adjust(top=0.83)
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/optimization/opt_skew_throughput.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

        # Latency with 0.1% queries deleg:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["Dataset"] == "Zipf") & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "DeSS") | (perfdf["Algorithm"] == "DeSS+MH"))], markersize=10,
                     linewidth=7, markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Skew")
        plt.ylabel(r"Latency ($\mu$sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/optimization/opt_skew_latency.svg"
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
    phis = [0.0001]
    df_max_uniques = [64]
    df_max_sums = [1000]
    streamlens=[30000000]
    query_rates = [1000]
    threads=[4,8,12,16,20,24]
    names=["spacesaving deleg","spacesaving deleg_maxheap"]
    datasets = ["", "flows_dirA", "flows_dirB"]
    fancy_dataset_names = ["Zipf, a=1", "Flows DirA", "Flows DirB"]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,[1],phis,"phiqr",datasets,"threads",True)

    print(perfdf)
    if throughput:
        # Throughput with 0.1% queries deleg diff threads:
        fig, ax = plt.subplots()
        sns.lineplot(x="Threads", y="Throughput", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "DeSS") | (perfdf["Algorithm"] == "DeSS+MH"))], markersize=10,
                     linewidth=7,style="Dataset", markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Threads")
        plt.ylabel(r"Throughput (Mops/sec)")
        plt.tight_layout()
        handles, labels = ax.get_legend_handles_labels()
        handles = handles[4:]
        labels=labels[4:]
        ax.axes.get_legend().remove()
        leg=fig.legend(handles, labels, loc="upper center", ncol=1)
        for legobj in leg.legendHandles:
            legobj.set_linewidth(7)
            legobj.set_markevery(0.01)
            legobj.set_markersize(60)
        #plt.subplots_adjust(top=0.83)
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/optimization/opt_threads_throughput.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

        # Latency with 0.1% queries deleg diff threads:
        fig, ax = plt.subplots()
        sns.lineplot(x="Threads", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.1) & (perfdf["phi"] == 0.0001) & ((perfdf["Algorithm"] == "DeSS") | (perfdf["Algorithm"] == "DeSS+MH"))], markersize=10,
                     linewidth=7,style="Dataset", markers=True, hue="Algorithm", palette="muted", ax=ax)
        plt.xlabel("Threads")
        plt.ylabel(r"Latency ($\mu$sec)")
        plt.tight_layout()
        ax.axes.get_legend().remove()
        ax.yaxis.grid(True,linestyle="--")
        name = "/home/victor/git/Delegation-Space-Saving/plots/optimization/opt_threads_latency.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()
