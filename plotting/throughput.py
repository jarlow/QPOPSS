import matplotlib
from matplotlib import pyplot as plt
import seaborn as sns
import numpy as np
import pandas as pd
import glob
from functools import partial
from matplotlib.lines import Line2D
from math import log10,floor
from plotters import generate_legend, create_parent_dir_if_not_exists,average_and_std, output_plot,parse_throughput,format_float, \
                        RUNTIME,names,fancy_names,datasets,fancy_dataset_names, set_opacity,showplots_flag,saveplots_flag, marker_styles

matplotlib.rcParams['figure.figsize'] = (9, 7)  # Dense info resolution
plt.rc('legend', fontsize=26)
matplotlib.rcParams.update({'font.size': 26})

# What parameter should be varied?
vs_dfu_dfs = False
vs_phi_qr = True
vt_phi_qr = True

# What is the metric?
throughput = True

def crate_performance_results_df(algorithm_names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,skew_rates,phis,experiment_name,dataset_names,x_axis_name,speedup_flag,maxheap_flag, path_prefix="throughput/"):
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
                                            globname="logs/" + path_prefix + x_axis_name + "_cm_topkapi_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                        elif n == ["prif"]:
                                            globname="logs/" + path_prefix + x_axis_name + "_prif_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                        else:
                                            if maxheap_flag:
                                                globname="logs/" + path_prefix + x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_maxheap_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                            else:
                                                globname="logs/" + path_prefix + x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                        print(globname)
                                        file=glob.glob(globname)[0]
                                        data = parse_throughput(file)
                                        tpavg, tpstd = average_and_std(data)      
                                        speedup = np.nan
                                        if speedup_flag and not n==["topkapi"] and not n==["prif"]:
                                            if n[1] == "deleg_min_max_heap":
                                                single_throughput = perfdf[
                                                        (perfdf["Zipf Parameter"] == z) & 
                                                        (perfdf["Query Rate"] == float(0 if qr==0 else float(qr)/(10000))) & 
                                                        (perfdf["Algorithm"] == "QOSS") & 
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


# Show influence of df_s and df_u over skew
if vs_dfu_dfs:
    ''' Variables: '''
    phis = [0.0001]
    df_max_uniques = [16, 32, 64, 128]
    df_max_sums = [100, 1000, 10000, 100000]
    streamlens=[10000000]
    query_rates = [100]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    ''' ########## '''

    perfdf=crate_performance_results_df(["spacesaving deleg_min_max_heap"],streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"dfsdfu",[""],"skew",False,False,"throughput/vsdfsdfu/")
    if throughput:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf, markersize=24, linewidth=7, markers=True,
                     style=perfdf["df_u"], hue=perfdf["df_s"], palette="muted", legend="full", ax=ax)
        ax2 = plt.axes([0.19, 0.40, .2, .2])
        sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf, markersize=24, linewidth=7, markers=True,
                     style=perfdf["df_u"], hue=perfdf["df_s"], palette="muted", legend=False, ax=ax2)
        ax2.set_title('zoom')
        ax2.set_xlim([0.45, 1.05])
        ax2.set_ylim([5, 13])

        ax.axes.get_legend().remove()
        ax2.set_xlabel('')
        ax2.set_ylabel('')
        ax2.set_xticks([0.5, 0.75, 1])
        #ax.indicate_inset_zoom(ax2, edgecolor="black")
        ax.set_xlabel("Skew")
        ax.set_ylabel("Throughput (Million Inserts/sec)")
        fig.legend(ncols=2, bbox_to_anchor=(0.9, 0.43), loc="center right")
        name = "plots/throughput/vsdfudfs/vs_performance_throughput_final_dfu_dfs.svg"
        if saveplots_flag:
            create_parent_dir_if_not_exists(name)
            plt.savefig(name, format="svg", dpi=4000)
        if showplots_flag:
            plt.show()
        plt.clf()
        plt.cla()
        plt.close()

def generate_plot(dataset,ylim_throughput_max,ylim_speedup_max,xticks,xticklabels,xaxis_string,query_rate,ax1_legend_function):
    fig, ax = plt.subplots(2, gridspec_kw={'height_ratios': [3, 1]},sharex=True)
    ax1 = ax[0]
    ax2 = ax[1]
    lineplot = sns.lineplot(x=xaxis_string, y="Throughput", data=perfdf[(perfdf["Query Rate"] == query_rate) & 
                                                                        (perfdf["Dataset"] == dataset) & 
                                                                        (perfdf["Threads"] != 1) &
                                                                        (perfdf["Algorithm"] != "QOSS")
                                                                        ],
                            markersize=24,linewidth=7, markers=marker_styles["phi"], style=r"$\phi$", 
                            hue="Algorithm", palette="muted", legend='brief' if query_rate == 0 else False, ax=ax1)
    ax1.set_xlabel(xaxis_string)
    ax1.set_ylabel("Million Inserts/sec")
    ax1.set_ylim(0, ylim_throughput_max)
    ax1.set_xticks(xticklabels)
    ax1.set_xticklabels(xticks)
    ax2.yaxis.set_label_position("left")
    if query_rate == 0:
        ax1_legend_function(lineplot)
    lineplot2 = sns.lineplot(x=xaxis_string, y="Speedup", data=perfdf[(perfdf["Query Rate"] == query_rate) & 
                                                                      (perfdf["Dataset"] == dataset) & 
                                                                      (perfdf["Threads"] != 1) &
                                                                      (perfdf["Algorithm"] != "QOSS")
                                                                      ], 
                             markersize=24, linewidth=7, markers=marker_styles["phi"],style=r"$\phi$", 
                             hue="Query Rate", palette=['black'], ax=ax2, legend=False)

    ax2.set_xticks(xticks)
    ax2.set_xticklabels(xticklabels)
    ax2.set_ylabel("Speedup\n QPOPSS/QOSS")
    ax2.set_ylim(0,ylim_speedup_max)
    set_opacity(lineplot2.get_lines(), 0.80)
    name = "plots/throughput/"+dataset+"_"+xaxis_string+"_performance_throughput_finalphiqr_"+str(query_rate)+"_quer.svg"
    output_plot(plt, name, showplots_flag, saveplots_flag)

def throughput_plots(dataset,ylim_throughput_max,ylim_speedup_max,xticks,xticklabels,xaxis_string):
    #Throughput with 0 queries:

    legend_fun = partial(generate_legend, location='best')
    generate_plot(dataset,ylim_throughput_max,ylim_speedup_max,xticks,xticklabels,xaxis_string,0,legend_fun)
    generate_plot(dataset,ylim_throughput_max,ylim_speedup_max,xticks,xticklabels,xaxis_string,0.01,legend_fun)
    generate_plot(dataset,ylim_throughput_max,ylim_speedup_max,xticks,xticklabels,xaxis_string,0.02,legend_fun)

# Vary skew, phi and query rate
if vs_phi_qr:
    ''' Variables: '''
    phis = [0.001, 0.0002, 0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens=[10000000]
    query_rates = [0,100,200]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phiqr",datasets,"skew",True,False, "throughput/vs/")
    if throughput:
        throughput_plots("Zipf",1000,36,[0.5,1,1.5,2,2.5,3],np.arange(0.5,3.5,0.5),"Zipf Parameter")

# Vary threads, phi and query rate
if vt_phi_qr:
    ''' Variables: '''
    phis = [0.001, 0.0002, 0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens=[10000000]
    query_rates = [0,100,200]
    threads=[4,8,12,16,20,24]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,[1.25],phis,"phiqr",datasets,"threads",True,False, "throughput/vt/")
    if throughput:
        throughput_plots("CAIDA Flows DirA",220,17,threads,np.arange(4, 28, 4),"Threads")
        throughput_plots("CAIDA Flows DirB",220,17,threads,np.arange(4, 28, 4),"Threads")