from matplotlib import pyplot as plt
import seaborn as sns
import matplotlib.ticker as plticker
import numpy as np
import pandas as pd
import matplotlib
import glob
from matplotlib.lines import Line2D
from math import log10,ceil,floor
from plotters import average_and_std,parse_latency,format_float,RUNTIME,names,fancy_names,datasets,fancy_dataset_names,showplots_flag,saveplots_flag

# What parameter should be varied?
vs_phi_qr = True
vt_phi_qr = True

# What is the metric?
latency = True

def crate_performance_results_df(algorithm_names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,skew_rates,phis,experiment_name,dataset_names,x_axis_name,speedup_flag,maxheap_flag):
    columns = ["Zipf Parameter", "Algorithm","Latency","Streamlength",
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
                thrs = [1]*len(threads)
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
                                            if maxheap_flag:
                                                globname="logs/"+x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_maxheap_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_latency.log"
                                            else:
                                                globname="logs/"+x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_latency.log"
                                        print(globname)
                                        file=glob.glob(globname)[0]
                                        data = parse_latency(file)
                                        tpavg, tpstd = average_and_std(data)      
                                        speedup = np.nan
                                        if speedup_flag and not n==["topkapi"]:
                                            if n[1] == "deleg":
                                                single_throughput = perfdf[
                                                        (perfdf["Zipf Parameter"] == z) & 
                                                        (perfdf["Query Rate"] == float(0 if qr==0 else float(qr)/(10000))) & 
                                                        (perfdf["Algorithm"] == "SeqSS") & 
                                                        (perfdf["Dataset"] == fancy_dataset_names[datasets.index(ds)]) &
                                                        (perfdf["phi"] == float(phi))]
                                                single_throughput=single_throughput.Throughput.values[0]
                                                speedup = float(float(tpavg) / float(single_throughput))
                                        if algname.find("single") != -1:
                                            for _thread in threads:
                                                    queryRate=float(0 if qr==0 else float(qr)/(10000))
                                                    perfdf.loc[len(perfdf.index)] = [z, fancy_names[names.index(algname)], float(tpavg), float(tpavg)*1000000*RUNTIME, 
                                                            float(tpavg)*RUNTIME*1000000*float(qr)*1/10000,df_max_sum,df_max_unique,float(phi),r"${scale}\times 10^{{-{exp}}}$".format(scale=int(phi*(10**(-floor(log10(phi))))),exp=-floor(log10(phi))),   # r"$\frac{{1}}{{{num}}}$".format(num=int(10**-log10(phi))),
                                                            queryRate,speedup,fancy_dataset_names[datasets.index(ds)],int(_thread)]

                                        queryRate=float(0 if qr==0 else float(qr)/(10000))
                                        perfdf.loc[len(perfdf.index)] = [z, fancy_names[names.index(algname)], float(tpavg), float(tpavg)*1000000*RUNTIME, 
                                                                        float(tpavg)*RUNTIME*1000000*float(qr)*1/10000,df_max_sum,df_max_unique,float(phi),r"${scale}\times 10^{{-{exp}}}$".format(scale=int(phi*(10**(-floor(log10(phi))))),exp=-floor(log10(phi))),#r"$\frac{{1}}{{{num}}}$".format(num=int(10**-log10(phi))),
                                                                        queryRate,speedup,fancy_dataset_names[datasets.index(ds)],int(t)]
    return perfdf


# Vary skew, phi and query rate
if vs_phi_qr:
    ''' Variables: '''
    phis = [0.001, 0.0002, 0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens=[10000000]
    query_rates = [100, 200]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phiqr",datasets,"vs",False,True)
    if latency:
        matplotlib.rcParams['figure.figsize'] = (6, 5.5) 
        plt.rc('legend', fontsize=26)
        matplotlib.rcParams.update({'font.size': 26})
        # Latency with 0.1% queries single:
        fig, ax1 = plt.subplots()
        palette = sns.color_palette(palette="muted")
        #Only orange and green
        palette = [ (0.9333333333333333, 0.5215686274509804, 0.2901960784313726), (0.41568627450980394, 0.8, 0.39215686274509803)]
        lineplot=sns.lineplot(x="Zipf Parameter", y="Latency", data=perfdf[
                            (perfdf["Query Rate"] == 0.01) & 
                            (perfdf["Dataset"] == "Zipf") & 
                            (perfdf["Algorithm"] != "SeqSS") &
                            (perfdf["phi"] == 0.0001)], markersize=13,
                     linewidth=7, linestyle="dashed", marker="s", hue="Algorithm", palette=palette, ax=ax1, legend=False)
        ax1.set_xlabel("Skew")
        ax1.set_ylabel(r"Latency $\mu$sec (log scale)")
        ax1.set_yscale("log")
        ax1.set_ylim(10,40000)
        ax1.set_xticks(np.arange(0.5,3.5,0.5))
        ax1.set_xticklabels(np.arange(0.5,3.5,0.5))
        name = "/home/victor/git/Delegation-Space-Saving/plots/skew_latency_0.0001phi_0.01query.svg"
        plt.tight_layout()
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
    phis = [0.001, 0.0002,0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens=[10000000]
    query_rates = [100, 200]
    threads=[4,8,12,16,20,24]

    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,[1.25],phis,"phiqr",datasets,"threads",False,True)

    if latency:
        matplotlib.rcParams['figure.figsize'] = (6, 5.5) 
        plt.rc('legend', fontsize=26)
        matplotlib.rcParams.update({'font.size': 26})

        # Latency with 0.1% queries deleg diff threads:

        fig, ax1 = plt.subplots()
        palette = sns.color_palette(palette="muted")
        #Only orange and green
        palette = [ (0.9333333333333333, 0.5215686274509804, 0.2901960784313726), (0.41568627450980394, 0.8, 0.39215686274509803)]
        lineplot = sns.lineplot(x="Threads", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.01) & 
                                                                        (perfdf["Dataset"] == "Zipf") & 
                                                                        (perfdf["Algorithm"] != "SeqSS") &
                                                                        (perfdf["Threads"] != 1) & 
                                                                        (perfdf["phi"] == 0.0001)
                                                                    ], 
                        markersize=13, linewidth=7, linestyle="dashed", marker="s",
                         hue="Algorithm", palette=palette, ax=ax1)
        ax1.set_yscale("log")
        ax1.set_xlabel("Threads")
        ax1.set_ylabel(r"Latency $\mu$sec (log scale)")
        ax1.set_ylim(10,40000)
        ax1.set_xticks(np.arange(4, 28, 4))
        ax1.set_xticklabels(np.arange(4, 28, 4))


        leg=lineplot.legend(fontsize=24,
        #bbox_to_anchor=(0.73, 0.53 ,0.3,0.5),
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
        name = "/home/victor/git/Delegation-Space-Saving/plots/threads_latency_0.0001phi_0.01query.svg"
        if saveplots_flag:
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()
