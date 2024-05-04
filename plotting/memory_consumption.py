import matplotlib.pyplot as plt
import numpy as np
import matplotlib
from mpl_toolkits.mplot3d import Axes3D
from matplotlib import cm
import matplotlib.ticker as mticker
import pandas as pd
import seaborn as sns
from plotters import generate_legend, saveplots_flag,showplots_flag, output_plot, marker_styles

COUNTER_BYTES = 32

def bytes_to_megabytes(bytes):
    return bytes * 10**-6

def delegation_filters_overhead_megabytes(T, df_u):
    return bytes_to_megabytes((40*(T**2)+4*(T**2)*2*df_u + 8 * T + 40*T))
def PRIF_queue_overhead_megabytes():
    return 100
def QPOPSS_counters(eps):
    return eps**(-1)
def QPOPSS_megabytes(T, eps, df_u=0):
    return bytes_to_megabytes(COUNTER_BYTES * QPOPSS_counters(eps)) + \
            delegation_filters_overhead_megabytes(T, df_u)
def PRIF_counters(T, eps, beta):
    return 2*((T+1)/(eps-beta))
def PRIF_megabytes(T, eps, beta):
    return bytes_to_megabytes(COUNTER_BYTES * PRIF_counters(T, eps, beta)) + PRIF_queue_overhead_megabytes()
def TOPKAPI_counters(T,eps):
    return QPOPSS_counters(eps) * T
def TOPKAPI_megabytes(T,eps):
    return bytes_to_megabytes(COUNTER_BYTES * TOPKAPI_counters(T,eps))

def plot_overhead():
    ''' plots the overhead memory used by Delegation Space-Saving '''
    threads = range(1, 128)
    df_us = range(1, 128)

    X, Y = np.meshgrid(threads, df_us)

    zs = np.array([delegation_filters_overhead_megabytes(x, y) for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z = zs.reshape(X.shape)
    print(list(zs))
    fig, ax = plt.subplots(subplot_kw={"projection": "3d"})
    ax.plot_surface(X, Y, Z, cmap=plt.cm.viridis, linewidth=0.2)
    ax.set_xlabel("Threadzz", linespacing=3.2)
    ax.set_ylabel(r"$df_u$", linespacing=3.2)
    ax.set_zlabel("Overhead memory consumed (MB)", linespacing=3.2)
    ax.xaxis.labelpad = 20
    ax.yaxis.labelpad = 20
    ax.zaxis.labelpad = 20
    colmap = cm.ScalarMappable(cmap=plt.cm.viridis)
    colmap.set_array(zs)
    fig.colorbar(colmap, ax=ax, location="right", shrink=0.7, pad=0)
    name = "plots/mem_overhead.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.show()
    plt.cla()
    plt.clf()

def log_tick_formatter(val, pos=None):
    return f"$10^{{{int(val)}}}$"

def plot_memory_usage3d():
    '''Plot memory usage by Space-Saving and Delegation Space-Saving'''
    threads = range(1, 128)
    skew = np.arange(1, 3.1, 0.1)
    phi = 0.0001
    eps_ratio = 0.1
    beta_ratio = 0.1
    eps = phi*eps_ratio
    beta = eps*beta_ratio

    X, Y = np.meshgrid(threads, skew)

    zs_qpopss = np.array([QPOPSS_megabytes(x, y, eps, df_u=16) for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z_QPOPSS = zs_qpopss.reshape(X.shape)
    zs_prif = np.array([PRIF_megabytes(x, eps, beta)
                    for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z_PRIF = zs_prif.reshape(X.shape)
    zs_topkapi = np.array([TOPKAPI_megabytes(x, y, eps)
                    for x, y in zip(np.ravel(X), np.ravel(Y))])
    Z_TOPKAPI = zs_topkapi.reshape(X.shape)

    fig, ax = plt.subplots(subplot_kw={"projection": "3d"})
    log10_qpopss = np.log10(Z_QPOPSS)
    log10_prif = np.log10(Z_PRIF)
    log10_topkapi = np.log10(Z_TOPKAPI)
    ax.plot_surface(X, Y, log10_qpopss, cmap=plt.cm.viridis, linewidth=0.2,
                    vmin=np.amin(log10_qpopss-1), vmax=np.amax(log10_prif+1))
    ax.plot_surface(X, Y, log10_prif,  cmap=plt.cm.viridis, linewidth=0.2,
                    vmin=np.amin(log10_qpopss-1), vmax=np.amax(log10_prif+1), shade=False, alpha=0.7)
    ax.plot_surface(X, Y, log10_topkapi,  cmap=plt.cm.viridis, linewidth=0.2,
                    vmin=np.amin(log10_qpopss-1), vmax=np.amax(log10_prif+1), shade=False, alpha=0.7)
    ax.set_xlabel("Threads", linespacing=3.2)
    ax.set_ylabel("Skew", linespacing=3.2)
    ax.set_zlabel("Memory consumption (MB)", linespacing=3.2)
    ax.xaxis.labelpad = 5
    ax.yaxis.labelpad = 5
    ax.zaxis.labelpad = 5
    #ax.zaxis._set_scale('log')
    ax.zaxis.set_major_formatter(mticker.FuncFormatter(log_tick_formatter))
    ax.zaxis.set_major_locator(mticker.MaxNLocator(integer=True))
    colmap = cm.ScalarMappable(cmap=plt.cm.viridis)
    colmap.set_array([zs_prif])
    #fig.colorbar(colmap, ax=ax, location="right", shrink=0.7, pad=0)
    ax.view_init(azim=120, elev=15) # rotate plot
    name = "plots/mem_usage_3d.svg"
    if saveplots_flag:
        plt.savefig(name, format="svg", dpi=4000)
    if showplots_flag:
        plt.gcf()
        plt.show()
    plt.cla()
    plt.clf()

def plot_memory_usage_2d():
    matplotlib.rcParams['figure.figsize'] = (7, 7.5) 
    plt.rc('legend', fontsize=26)
    matplotlib.rcParams.update({'font.size': 26})
    '''Plot memory usage by QPOPSS when compared to PRIF'''
    phis = [0.0001, 0.0002, 0.001]
    eps_ratio = 0.1
    beta_ratio = 0.9
    threads = range(1, 512, 64)
    
    fig, ax = plt.subplots()
    df = pd.DataFrame(columns=['phi', 'threads', 'memory', 'Algorithm'])
    for phi in phis:
        eps = phi*eps_ratio
        beta = eps*beta_ratio
        for t in threads:
            zs_qpopss = QPOPSS_megabytes(t, eps, df_u=16)
            zs_prif = PRIF_megabytes(t, eps, beta)
            zs_topkapi = TOPKAPI_megabytes(t, eps)
            df = df._append({'phi': phi, 'threads': t, 'memory': zs_qpopss, 'Algorithm': 'QPOPSS'}, ignore_index=True)
            df = df._append({'phi': phi, 'threads': t, 'memory': zs_topkapi, 'Algorithm': 'TOPKAPI'}, ignore_index=True)
            df = df._append({'phi': phi, 'threads': t, 'memory': zs_prif, 'Algorithm': 'PRIF'}, ignore_index=True)
    df['phi'] = df['phi'].astype(str)
    df['threads'] = df['threads'].astype(int)
    df['memory'] = df['memory'].astype(float)
    df['Algorithm'] = df['Algorithm'].astype(str)
            
    lineplot = sns.lineplot(data=df, x='threads', y='memory', style='phi', hue='Algorithm', ax=ax, palette='muted',
                 markers=marker_styles["phi"], markersize=24, linewidth=7)
    generate_legend(lineplot)
    ax.set_xlabel("Threads")
    ax.set_ylabel("Memory consumption (MB)")
    ax.set_yscale('log')
    name = "plots/mem_usage_2d.svg"
    output_plot(plt,name,showplots_flag, saveplots_flag)
    

#plot_overhead()
#plot_memory_usage3d()
plot_memory_usage_2d()