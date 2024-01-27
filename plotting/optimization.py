from matplotlib import pyplot as plt
import seaborn as sns
import pandas as pd
import matplotlib
import glob
from math import log10
from plotters import average_and_std, generate_legend, output_plot,parse_latency,format_float,parse_throughput, \
                        RUNTIME,names,fancy_names,datasets,fancy_dataset_names,showplots_flag,saveplots_flag, marker_styles

matplotlib.rcParams['figure.figsize'] = (6, 5) 
plt.rc('legend', fontsize=26)
matplotlib.rcParams.update({'font.size': 26})

#colors for the optimization plots, where the "inner algorithm" is varied
WITH_QOSS_COLOR=sns.palettes.SEABORN_PALETTES["muted"][0] #Same as other plots
WITH_SS_COLOR=sns.palettes.SEABORN_PALETTES["muted"][3] #New color
OPTIMIZATION_COLORS = [WITH_QOSS_COLOR,WITH_SS_COLOR]


# What parameter should be varied?
vs_phi_qr = True
vt_phi_qr = True

# What is the metric?
throughput = True
speedupthroughput = True
fancy_names = ["With QOSS","With SS"]
def crate_performance_results_df(algorithm_names,streamlens,query_rates,
                                 df_max_uniques,df_max_sums,threads,skew_rates,
                                 phis,experiment_name,dataset_names,x_axis_name,latency_flag,path_prefix="throughput/"):
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
                                            globname="logs/" + path_prefix + x_axis_name+"_cm_topkapi_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"
                                        else:
                                            globname="logs/" + path_prefix + x_axis_name+"_cm_"+n[0]+"_"+n[1]+"_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(qr)+"_"+experiment_name+ds+"_throughput.log"                                     
                                        print(globname)
                                        file=glob.glob(globname)[0]
                                        throughput = parse_throughput(file)
                                        throughputavg, _ = average_and_std(throughput)
                                        latencyavg = 0
                                        if latency_flag and x_axis_name == "threads":
                                            globname="logs/latency/" + "vt" + "/" + "threads_cm_"+n[0]+"_"+n[1]+"_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(100)+"_"+experiment_name+ds+"_latency.log"
                                            print(globname)
                                            file=glob.glob(globname)[0]
                                            latency = parse_latency(file)
                                            latencyavg,_ = average_and_std(latency)
                                        elif latency_flag and x_axis_name == "skew":
                                            globname="logs/latency/" + "vs" + "/vs" + "_cm_"+n[0]+"_"+n[1]+"_throughput_"+str(t)+"_"+str(z)+"_"+str(format_float(phi))+"_"+str(df_max_sum)+"_"+str(df_max_unique)+"_"+str(N)+"_"+str(100)+"_"+experiment_name+ds+"_latency.log"
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
    phis = [0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens=[10000000]
    query_rates = [100]
    skew_rates = [0.5, 0.75, 1, 1.25, 1.5, 1.75, 2, 2.25, 2.5, 2.75, 3]
    names=["spacesaving deleg_min_heap","spacesaving deleg_min_max_heap"]
    fancy_names = ["With SS","With QOSS"]
    datasets=[""]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,[24],skew_rates,phis,"phiqr",datasets,"skew",True,"throughput/vs/")
    if throughput:
        fig, ax1 = plt.subplots()
        colors = [WITH_QOSS_COLOR,WITH_SS_COLOR]
        lineplot=sns.lineplot(x="Zipf Parameter", y="Throughput", data=perfdf[
                            (perfdf["Query Rate"] == 0.01) & 
                            (perfdf["Dataset"] == "Zipf") & 
                            (perfdf["phi"] == 0.0001) & 
                            ((perfdf["Algorithm"] == "With QOSS") | (perfdf["Algorithm"] == "With SS"))], 
                            markersize=24,linewidth=7, hue="Algorithm", palette=OPTIMIZATION_COLORS, ax=ax1, marker=marker_styles["phi"][2], legend='brief')
        plt.xlabel("Skew")
        plt.ylabel(r"Throughput (Mops/sec)")
        plt.yscale("log")
        generate_legend(lineplot,columns=1)
        name = "plots/optimization/opt_skew_throughput.svg"
        output_plot(plt, name, showplots_flag, saveplots_flag)

        # Latency with 0.1% queries deleg:
        fig, ax = plt.subplots()
        sns.lineplot(x="Zipf Parameter", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.01) & 
                                                                  (perfdf["Dataset"] == "Zipf") & 
                                                                  (perfdf["phi"] == 0.0001) & 
                                                                  ((perfdf["Algorithm"] == "With QOSS") | (perfdf["Algorithm"] == "With SS"))], 
                     markersize=24, linewidth=7, marker=marker_styles["phi"][2], hue="Algorithm", palette=OPTIMIZATION_COLORS, ax=ax, legend=False)
        plt.xlabel("Skew")
        plt.ylabel(r"Latency ($\mu$sec)")
        name = "plots/optimization/opt_skew_latency.svg"
        output_plot(plt, name, showplots_flag, saveplots_flag)

    
# Vary threads, phi and query rate
if vt_phi_qr:
    ''' Variables: '''
    phis = [0.0001]
    df_max_uniques = [16]
    df_max_sums = [1000]
    streamlens=[10000000]
    query_rates = [100]
    threads=[4,8,12,16,20,24]
    names=["spacesaving deleg_min_heap","spacesaving deleg_min_max_heap"]
    fancy_names = ["With SS","With QOSS"]
    datasets = ["", "flows_dirA", "flows_dirB"]
    fancy_dataset_names = ["Zipf, a=1.25", "Flows DirA", "Flows DirB"]
    ''' ########## '''
    perfdf=crate_performance_results_df(names,streamlens,query_rates,df_max_uniques,df_max_sums,threads,[1.25],phis,"phiqr",datasets,"threads",True, "throughput/vt/")

    if throughput:
        # Throughput with 0.1% queries deleg diff threads:
        fig, ax = plt.subplots()
        lineplot=sns.lineplot(x="Threads", y="Throughput", data=perfdf[
                                    (perfdf["Query Rate"] == 0.01) & 
                                    (perfdf["phi"] == 0.0001) & 
                                    ((perfdf["Algorithm"] == "With SS") | (perfdf["Algorithm"] == "With QOSS"))],
                     markersize=24, linewidth=7,style="Dataset", markers=marker_styles["datasets"], hue="Algorithm", palette=OPTIMIZATION_COLORS, ax=ax, legend="brief")
        plt.xlabel("Threads")
        plt.ylabel(r"Throughput (Mops/sec)")
        generate_legend(lineplot)
        
        # Remove first 4 handles and labels (i.e. the colors which have already been introduced in the twin plot)
        h, l = ax.get_legend_handles_labels()
        ax.legend(handles=[item for item in h[4:]], labels= [item for item in l[4:]])
        name = "plots/optimization/opt_threads_throughput.svg"
        output_plot(plt, name, showplots_flag, saveplots_flag)

        # Latency with 0.1% queries deleg diff threads:
        print(perfdf[(perfdf["Query Rate"] == 0.01) & 
                                    (perfdf["phi"] == 0.0001) & 
                                    ((perfdf["Algorithm"] == "With SS") | (perfdf["Algorithm"] == "With QOSS"))])
        fig, ax = plt.subplots()
        sns.lineplot(x="Threads", y="Latency", data=perfdf[(perfdf["Query Rate"] == 0.01) & 
                                                           (perfdf["phi"] == 0.0001) & 
                                                           ((perfdf["Algorithm"] == "With SS") | (perfdf["Algorithm"] == "With QOSS"))], 
                     markersize=24, linewidth=7, style="Dataset", markers=marker_styles["datasets"], hue="Algorithm", palette=OPTIMIZATION_COLORS, ax=ax, legend=False)
        plt.xlabel("Threads")
        plt.ylabel(r"Latency ($\mu$sec)")
        name = "plots/optimization/opt_threads_latency.svg"
        output_plot(plt, name, showplots_flag, saveplots_flag)
